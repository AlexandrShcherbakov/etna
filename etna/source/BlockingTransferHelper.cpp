#include <etna/BlockingTransferHelper.hpp>

#include <vulkan/vulkan_format_traits.hpp>

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>


namespace etna
{

BlockingTransferHelper::BlockingTransferHelper(CreateInfo info)
  : stagingSize{info.stagingSize}
  , stagingBuffer{etna::get_context().createBuffer(Buffer::CreateInfo{
      .size = stagingSize,
      .bufferUsage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
      .memoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
      .name = "BlockingTransferHelper::stagingBuffer",
    })}
{
  stagingBuffer.map();
}

void BlockingTransferHelper::uploadBuffer(
  OneShotCmdMgr& cmd_mgr, Buffer& dst, std::uint32_t offset, std::span<std::byte const> src)
{
  ETNA_VERIFYF(offset % 4 == 0 && src.size() % 4 == 0, "All GPU access must be 16-byte aligned!");


  for (vk::DeviceSize currPos = 0; currPos < src.size(); currPos += stagingSize)
  {
    const vk::DeviceSize batchSize =
      std::min(static_cast<vk::DeviceSize>(src.size() - currPos), stagingSize);

    std::memcpy(stagingBuffer.data(), src.data() + currPos, batchSize);

    auto cmdBuf = cmd_mgr.start();

    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      vk::BufferCopy2 copy{
        .srcOffset = 0,
        .dstOffset = offset + currPos,
        .size = batchSize,
      };

      vk::CopyBufferInfo2 info{
        .srcBuffer = stagingBuffer.get(),
        .dstBuffer = dst.get(),
        .regionCount = 1,
        .pRegions = &copy,
      };
      cmdBuf.copyBuffer2(info);
    }
    ETNA_CHECK_VK_RESULT(cmdBuf.end());

    cmd_mgr.submitAndWait(std::move(cmdBuf));
  }
}

void BlockingTransferHelper::readbackBuffer(
  OneShotCmdMgr& cmd_mgr, std::span<std::byte> dst, const Buffer& src, uint32_t offset)
{
  ETNA_VERIFYF(offset % 4 == 0 && dst.size() % 4 == 0, "All GPU access must be 16-byte aligned!");


  for (vk::DeviceSize currPos = 0; currPos < dst.size(); currPos += stagingSize)
  {
    const vk::DeviceSize batchSize =
      std::min(static_cast<vk::DeviceSize>(dst.size() - currPos), stagingSize);

    auto cmdBuf = cmd_mgr.start();

    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      vk::BufferCopy2 copy{
        .srcOffset = offset + currPos,
        .dstOffset = 0,
        .size = batchSize,
      };

      vk::CopyBufferInfo2 info{
        .srcBuffer = src.get(),
        .dstBuffer = stagingBuffer.get(),
        .regionCount = 1,
        .pRegions = &copy,
      };
      cmdBuf.copyBuffer2(info);
    }
    ETNA_CHECK_VK_RESULT(cmdBuf.end());

    cmd_mgr.submitAndWait(std::move(cmdBuf));

    std::memcpy(dst.data() + currPos, stagingBuffer.data(), batchSize);
  }
}

void BlockingTransferHelper::uploadImage(
  OneShotCmdMgr& cmd_mgr, Image& dst, uint32_t mip_level, uint32_t layer, std::span<std::byte const> src)
{
  auto [w, h, d] = dst.getExtent();

  const std::size_t bytesPerPixel = vk::blockSize(dst.getFormat());

  ETNA_ASSERTF(d == 1, "3D image uploads are not implemented yet!");

  ETNA_ASSERTF(
    w * h * bytesPerPixel == src.size(),
    "Image size mismatch between CPU and GPU! Expected {} bytes, but got {}!",
    w * h * bytesPerPixel,
    src.size());

  const std::size_t bytesPerLine = w * bytesPerPixel;
  const std::size_t linesPerUpload = stagingSize / bytesPerLine;
  ETNA_ASSERTF(
    linesPerUpload > 0,
    "Unable to fit a single line into the staging buffer! Buffer size is {} bytes, but a single line is {} bytes!",
    stagingSize,
    w * bytesPerPixel
  );

  for (std::size_t uploadedLines = 0; uploadedLines < h; uploadedLines += linesPerUpload)
  {
    const std::size_t linesThisUpload = std::min(linesPerUpload, h - uploadedLines);
    std::memcpy(stagingBuffer.data(), src.data() + uploadedLines * bytesPerLine, linesThisUpload * bytesPerLine);

    auto cmdBuf = cmd_mgr.start();

    ETNA_CHECK_VK_RESULT(cmdBuf.begin(vk::CommandBufferBeginInfo{}));
    {
      if (uploadedLines == 0)
      {
        etna::set_state(
          cmdBuf,
          dst.get(),
          vk::PipelineStageFlagBits2::eTransfer,
          vk::AccessFlagBits2::eTransferWrite,
          vk::ImageLayout::eTransferDstOptimal,
          dst.getAspectMaskByFormat());
        etna::flush_barriers(cmdBuf);
      }

      vk::BufferImageCopy2 copy{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
          vk::ImageSubresourceLayers{
            .aspectMask = dst.getAspectMaskByFormat(),
            .mipLevel = mip_level,
            .baseArrayLayer = layer,
            .layerCount = 1,
          },
        .imageOffset = vk::Offset3D{0, static_cast<int32_t>(uploadedLines), 0},
        .imageExtent = vk::Extent3D{w, static_cast<uint32_t>(linesThisUpload), 1},
      };
      vk::CopyBufferToImageInfo2 info{
        .srcBuffer = stagingBuffer.get(),
        .dstImage = dst.get(),
        .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
        .regionCount = 1,
        .pRegions = &copy,
      };
      cmdBuf.copyBufferToImage2(info);

      if (uploadedLines + linesPerUpload >= h)
      {
        etna::set_state(
          cmdBuf,
          dst.get(),
          {},
          {},
          vk::ImageLayout::eShaderReadOnlyOptimal,
          dst.getAspectMaskByFormat());
        etna::flush_barriers(cmdBuf);
      }
    }
    ETNA_CHECK_VK_RESULT(cmdBuf.end());

    cmd_mgr.submitAndWait(std::move(cmdBuf));
  }
}

} // namespace etna
