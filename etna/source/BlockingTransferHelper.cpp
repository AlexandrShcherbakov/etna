#include <etna/BlockingTransferHelper.hpp>

#include <etna/GlobalContext.hpp>


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
  ETNA_ASSERTF(offset % 4 == 0 && src.size() % 4 == 0, "All GPU access must be 16-byte aligned!");


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
  ETNA_ASSERTF(offset % 4 == 0 && dst.size() % 4 == 0, "All GPU access must be 16-byte aligned!");


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

} // namespace etna
