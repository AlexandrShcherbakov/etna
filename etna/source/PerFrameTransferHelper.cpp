#include <etna/PerFrameTransferHelper.hpp>
#include <etna/Etna.hpp>

#include <vulkan/vulkan_format_traits.hpp>
#include <type_traits>

namespace etna
{

template <class T>
  requires std::is_integral_v<T>
static T align_up(T val, T alignment)
{
  return ((val - 1) / alignment + 1) * alignment;
}

template <class T>
  requires std::is_integral_v<T>
static T align_down(T val, T alignment)
{
  return (val / alignment) * alignment;
}

static constexpr auto BUFFER_ALIGNMENT = vk::DeviceSize{16};
static constexpr auto MAX_STAGING_SIZE = (1ull << 32) - 1; // For 32bit indices

static uint32_t offset3d_to_linear(vk::Offset3D o, vk::Extent3D e)
{
  return uint32_t(o.z) * e.height * e.width + uint32_t(o.y) * e.width + uint32_t(o.x);
}

static vk::Offset3D linear_to_offset3d(uint32_t l, vk::Extent3D e)
{
  return vk::Offset3D{
    int32_t(l % e.width), int32_t((l / e.width) % e.height), int32_t(l / (e.width * e.height))};
}

PerFrameTransferHelper::PerFrameTransferHelper(CreateInfo info)
  : inFrameState{ProcessingState::IDLE}
  , lastFrame{uint64_t(-1)}
  , stagingSize{align_down(info.totalStagingSize / info.wc->multiBufferingCount(), BUFFER_ALIGNMENT)}
  , curFrameStagingOffset{0}
  , stagingBuffer{*info.wc, [&](size_t) {
                    return etna::get_context().createBuffer(
                      Buffer::CreateInfo{
                        .size = stagingSize,
                        .bufferUsage = vk::BufferUsageFlagBits::eTransferDst |
                          vk::BufferUsageFlagBits::eTransferSrc,
                        .memoryUsage = VMA_MEMORY_USAGE_AUTO,
                        .allocationCreate = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                          VMA_ALLOCATION_CREATE_MAPPED_BIT,
                        .name = "PerFrameTransferHelper::stagingBuffer",
                      });
                  }}
  , wc{*info.wc}
{
  ETNA_VERIFYF(
    stagingSize > 0,
    "PerFrameTransferHelper: Total requested staging size {} is too small for multibuffering {}",
    info.totalStagingSize,
    wc.multiBufferingCount());
  ETNA_VERIFYF(
    stagingSize <= MAX_STAGING_SIZE,
    "PerFrameTransferHelper: total staging size can not exceed {} * multibuffering.",
    MAX_STAGING_SIZE);
  stagingBuffer.iterate([](auto& buf) { buf.map(); });
}

void PerFrameTransferHelper::UploadProcessor::finish()
{
  if (owner == nullptr)
    return;

  if (owner->inFrameState == ProcessingState::UPLOAD)
    owner->inFrameState = ProcessingState::UPLOAD_DONE;
  else
    ETNA_PANIC("PerFrameTransferHelper: upload scope must end before performing other actions.");
}

void PerFrameTransferHelper::ReadbackProcessor::finish()
{
  if (owner == nullptr)
    return;

  if (owner->inFrameState == ProcessingState::READBACK)
    owner->inFrameState = ProcessingState::READBACK_DONE;
  else
    ETNA_PANIC("PerFrameTransferHelper: readback scope must end before performing other actions.");
}

PerFrameTransferHelper::ReadbackProcessor PerFrameTransferHelper::FrameProcessor::beginReadback()
{
  ETNA_ASSERT(owner);

  if (owner->inFrameState != ProcessingState::READY)
    ETNA_PANIC("PerFrameTransferHelper: readbacks must be processed first.");

  owner->inFrameState = ProcessingState::READBACK;
  return PerFrameTransferHelper::ReadbackProcessor{owner};
}

PerFrameTransferHelper::UploadProcessor PerFrameTransferHelper::FrameProcessor::beginUpload()
{
  ETNA_ASSERT(owner);

  if (
    owner->inFrameState != ProcessingState::READY &&
    owner->inFrameState != ProcessingState::READBACK_DONE)
  {
    ETNA_PANIC("PerFrameTransferHelper: uploads must be done after readbacks.");
  }

  owner->inFrameState = ProcessingState::UPLOAD;
  owner->curFrameStagingOffset = 0;
  return PerFrameTransferHelper::UploadProcessor{owner};
}

void PerFrameTransferHelper::FrameProcessor::finish()
{
  if (owner == nullptr)
    return;

  if (
    owner->inFrameState == ProcessingState::READY ||
    owner->inFrameState == ProcessingState::READBACK_DONE ||
    owner->inFrameState == ProcessingState::UPLOAD_DONE)
  {
    owner->inFrameState = ProcessingState::IDLE;
    owner->lastFrame = owner->wc.batchIndex();
  }
  else
  {
    if (owner->inFrameState == ProcessingState::READBACK)
      ETNA_PANIC("PerFrameTransferHelper: readback not finished at scope end.");
    else if (owner->inFrameState == ProcessingState::UPLOAD)
      ETNA_PANIC("PerFrameTransferHelper: upload not finished at scope end.");
    else
      ETNA_PANIC("PerFrameTransferHelper: multiple scope ends.");
  }
}

PerFrameTransferHelper::FrameProcessor PerFrameTransferHelper::beginFrame()
{
  if (inFrameState != ProcessingState::IDLE)
    ETNA_PANIC("PerFrameTransferHelper: already processing frame.");
  else if (lastFrame == wc.batchIndex())
    ETNA_PANIC("PerFrameTransferHelper: already processed frame {}.", lastFrame);

  inFrameState = ProcessingState::READY;
  return PerFrameTransferHelper::FrameProcessor{this};
}

AsyncBufferUploadState PerFrameTransferHelper::initUploadBufferAsync(
  const Buffer& dst, uint32_t offset, std::span<std::byte const> src) const
{
  ETNA_VERIFYF(
    offset % 4 == 0 && src.size() % 4 == 0,
    "PerFrameTransferHelper: All GPU access must be 16-byte aligned!");
  return AsyncBufferUploadState{this, uint64_t(-1), &dst, offset, src};
}

AsyncBufferReadbackState PerFrameTransferHelper::initReadbackBufferAsync(
  std::span<std::byte> dst, const Buffer& src, uint32_t offset) const
{
  ETNA_VERIFYF(
    offset % 4 == 0 && dst.size() % 4 == 0,
    "PerFrameTransferHelper: All GPU access must be 16-byte aligned!");
  return AsyncBufferReadbackState{this, uint64_t(-1), &src, 0, 0, {}, 0, dst};
}

AsyncImageUploadState PerFrameTransferHelper::initUploadImageAsync(
  const Image& dst, uint32_t mip_level, uint32_t layer, std::span<std::byte const> src) const
{
  auto [w, h, d] = dst.getExtent();

  ETNA_VERIFYF(d == 1, "PerFrameTransferHelper: 3D image async uploads are not implemented yet!");

  const size_t bytesPerPixel = vk::blockSize(dst.getFormat());
  [[maybe_unused]] const size_t imagePixelCount = w * h * d;

  ETNA_VERIFYF(
    imagePixelCount * bytesPerPixel == src.size(),
    "PerFrameTransferHelper: Image size mismatch between CPU and GPU! Expected {} bytes, but got "
    "{}!",
    imagePixelCount * bytesPerPixel,
    src.size());

  return AsyncImageUploadState{
    this, uint64_t(-1), &dst, mip_level, layer, bytesPerPixel, vk::Offset3D{0, 0, 0}, src};
}

bool PerFrameTransferHelper::uploadBufferSync(
  vk::CommandBuffer cmd_buf, const Buffer& dst, uint32_t offset, std::span<std::byte const> src)
{
  ETNA_VERIFYF(
    offset % 4 == 0 && src.size() % 4 == 0,
    "PerFrameTransferHelper: All GPU access must be 16-byte aligned!");

  vk::DeviceSize stagingOffset = align_up(curFrameStagingOffset, BUFFER_ALIGNMENT);
  if (stagingSize - stagingOffset < src.size())
    return false;

  memcpy(stagingBuffer.get().data() + stagingOffset, src.data(), src.size());
  transferBufferRegion(cmd_buf, stagingBuffer.get(), dst, stagingOffset, offset, src.size());

  curFrameStagingOffset = stagingOffset + src.size();
  return true;
}

bool PerFrameTransferHelper::uploadImageSync(
  vk::CommandBuffer cmd_buf,
  const Image& dst,
  uint32_t mip_level,
  uint32_t layer,
  std::span<std::byte const> src)
{
  auto [w, h, d] = dst.getExtent();

  const size_t bytesPerPixel = vk::blockSize(dst.getFormat());
  [[maybe_unused]] const size_t imagePixelCount = w * h * d;

  ETNA_VERIFYF(
    imagePixelCount * bytesPerPixel == src.size(),
    "PerFrameTransferHelper: Image size mismatch between CPU and GPU! Expected {} bytes, but got "
    "{}!",
    imagePixelCount * bytesPerPixel,
    src.size());

  vk::DeviceSize stagingOffset = align_up(curFrameStagingOffset, vk::DeviceSize{bytesPerPixel});
  if (stagingSize - stagingOffset < src.size())
    return false;

  memcpy(stagingBuffer.get().data() + stagingOffset, src.data(), src.size());

  etna::set_state(
    cmd_buf,
    dst.get(),
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eTransferDstOptimal,
    dst.getAspectMaskByFormat());
  etna::flush_barriers(cmd_buf);

  uploadImageRect(
    cmd_buf, dst, mip_level, layer, vk::Offset3D{0, 0, 0}, vk::Extent3D{w, h, d}, stagingOffset);

  curFrameStagingOffset = stagingOffset + src.size();
  return true;
}

bool PerFrameTransferHelper::progressBufferUploadAsync(
  vk::CommandBuffer cmd_buf, AsyncBufferUploadState& state)
{
  ETNA_VERIFYF(
    state.lastFrame != wc.batchIndex(),
    "PerFrameTransferHelper: Attempting to upload the same buffer twice on frame {}",
    state.lastFrame);
  ETNA_ASSERT(state.transferHelper == this);

  if (state.done())
    return true;

  vk::DeviceSize stagingOffset = align_up(curFrameStagingOffset, BUFFER_ALIGNMENT);
  size_t transferSize =
    std::min(state.src.size(), align_down(stagingSize - stagingOffset, BUFFER_ALIGNMENT));
  if (transferSize == 0)
    return false;

  memcpy(stagingBuffer.get().data() + stagingOffset, state.src.data(), transferSize);
  transferBufferRegion(
    cmd_buf, stagingBuffer.get(), *state.dst, stagingOffset, state.offset, transferSize);

  curFrameStagingOffset = stagingOffset + transferSize;
  state.offset += uint32_t(transferSize);
  state.src = state.src.subspan(transferSize);

  state.lastFrame = wc.batchIndex();

  return state.done();
}

bool PerFrameTransferHelper::progressBufferReadbackAsync(
  vk::CommandBuffer cmd_buf, AsyncBufferReadbackState& state)
{
  ETNA_VERIFYF(
    state.lastFrame != wc.batchIndex(),
    "PerFrameTransferHelper: Attempting to readback the same buffer twice on frame {}",
    state.lastFrame);
  ETNA_ASSERT(state.transferHelper == this);

  if (state.done())
    return true;

  [[maybe_unused]] int fulfilledIssues = 0;
  for (auto& issue : state.issues)
  {
    if (issue.fulfillmentFrame == wc.batchIndex())
    {
      ETNA_ASSERT(fulfilledIssues == 0); // we should be collecting exactly one readback/frame

      memcpy(state.dst.data(), stagingBuffer.get().data() + issue.offset, issue.size);

      ++fulfilledIssues;
      state.dst = state.dst.subspan(issue.size);
      state.issuedOffset += issue.size;

      issue = {};

      ETNA_ASSERT(state.issuedOffset <= state.remainingOffset);
    }
    else if (issue.fulfillmentFrame < wc.batchIndex())
    {
      ETNA_PANIC(
        "PerFrameTransferHelper: missed buffer readback due on frame {} while calling "
        "progressBufferReadbackAsync only on frame {}. Always progress all outstanding readbacks!",
        issue.fulfillmentFrame,
        wc.batchIndex());
    }
  }

  if (state.dst.empty())
    return true;
  else if (state.remainingOffset - state.issuedOffset >= state.dst.size())
    return false;

  vk::DeviceSize stagingOffset = align_up(curFrameStagingOffset, BUFFER_ALIGNMENT);
  size_t transferSize =
    std::min(state.dst.size(), align_down(stagingSize - stagingOffset, BUFFER_ALIGNMENT));
  if (transferSize == 0)
    return false;

  transferBufferRegion(
    cmd_buf, *state.src, stagingBuffer.get(), state.remainingOffset, stagingOffset, transferSize);

  curFrameStagingOffset = stagingOffset + transferSize;

  auto& issue = state.issues[state.nextIssueSlot];
  ETNA_ASSERT(issue == AsyncBufferReadbackState::Issue{}); // Slot must be empty by this frame

  issue.fulfillmentFrame = wc.batchIndex() + wc.multiBufferingCount();
  issue.size = uint32_t(transferSize);
  issue.offset = uint32_t(stagingOffset);

  state.remainingOffset += issue.size;
  state.nextIssueSlot = (state.nextIssueSlot + 1) % state.issues.size();

  state.lastFrame = wc.batchIndex();

  return false;
}

bool PerFrameTransferHelper::progressImageUploadAsync(
  vk::CommandBuffer cmd_buf, AsyncImageUploadState& state)
{
  ETNA_VERIFYF(
    state.lastFrame != wc.batchIndex(),
    "PerFrameTransferHelper: Attempting to upload the same image twice on frame {}",
    state.lastFrame);
  ETNA_ASSERT(state.transferHelper == this);

  if (state.done())
    return true;

  vk::DeviceSize stagingOffset =
    align_up(curFrameStagingOffset, vk::DeviceSize{state.bytesPerPixel});
  size_t transferSize = std::min(
    state.src.size(), align_down(stagingSize - stagingOffset, vk::DeviceSize{state.bytesPerPixel}));
  if (transferSize == 0)
    return false;

  memcpy(stagingBuffer.get().data() + stagingOffset, state.src.data(), transferSize);

  if (state.offset == vk::Offset3D{0, 0, 0})
  {
    etna::set_state(
      cmd_buf,
      state.dst->get(),
      vk::PipelineStageFlagBits2::eTransfer,
      vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eTransferDstOptimal,
      state.dst->getAspectMaskByFormat());
    etna::flush_barriers(cmd_buf);
  }

  uploadImageRegion(
    cmd_buf,
    *state.dst,
    state.mipLevel,
    state.layer,
    state.bytesPerPixel,
    state.offset,
    stagingOffset,
    transferSize);

  vk::Extent3D imageExtent = state.dst->getExtent();
  curFrameStagingOffset = stagingOffset + transferSize;
  state.offset = linear_to_offset3d(
    uint32_t(offset3d_to_linear(state.offset, imageExtent) + transferSize / state.bytesPerPixel),
    imageExtent);
  state.src = state.src.subspan(transferSize);

  state.lastFrame = wc.batchIndex();

  return state.done();
}

void PerFrameTransferHelper::transferBufferRegion(
  vk::CommandBuffer cmd_buf,
  const Buffer& src,
  const Buffer& dst,
  vk::DeviceSize src_offset,
  vk::DeviceSize dst_offset,
  size_t size)
{
  vk::BufferCopy2 copy{
    .srcOffset = src_offset,
    .dstOffset = dst_offset,
    .size = size,
  };
  vk::CopyBufferInfo2 info{
    .srcBuffer = src.get(),
    .dstBuffer = dst.get(),
    .regionCount = 1,
    .pRegions = &copy,
  };
  cmd_buf.copyBuffer2(info);
}

void PerFrameTransferHelper::uploadImageRegion(
  vk::CommandBuffer cmd_buf,
  const Image& dst,
  uint32_t mip_level,
  uint32_t layer,
  size_t bytes_per_pixel,
  vk::Offset3D offset,
  vk::DeviceSize staging_offset,
  size_t size)
{
  vk::Extent3D imageExtent = dst.getExtent();
  size_t pixelsLeft = size / bytes_per_pixel;
  vk::Offset3D finalOffset =
    linear_to_offset3d(uint32_t(offset3d_to_linear(offset, imageExtent) + pixelsLeft), imageExtent);

  ETNA_ASSERT(imageExtent.depth == 1);
  ETNA_ASSERT(offset.z == 0);
  ETNA_ASSERT(
    finalOffset.z == 0 || (finalOffset.z == 1 && finalOffset.y == 0 && finalOffset.x == 0));

  auto skipLines = [&](vk::Offset3D off, uint32_t lines) {
    off.y += lines;
    off.x = 0;
    off.z += off.y / imageExtent.height;
    off.y = off.y % imageExtent.height;
    return off;
  };

  if (offset.x > 0)
  {
    const uint32_t firstLinePixels = std::min(imageExtent.width - offset.x, uint32_t(pixelsLeft));

    uploadImageRect(
      cmd_buf, dst, mip_level, layer, offset, vk::Extent3D{firstLinePixels, 1, 1}, staging_offset);

    pixelsLeft -= firstLinePixels;
    if (pixelsLeft == 0)
      return;

    staging_offset += firstLinePixels * bytes_per_pixel;
    offset = skipLines(offset, 1);
  }

  const uint32_t fullLines =
    (finalOffset.y - offset.y) + (finalOffset.z - offset.z) * imageExtent.height;
  ETNA_ASSERT(fullLines * imageExtent.width <= pixelsLeft);

  uploadImageRect(
    cmd_buf,
    dst,
    mip_level,
    layer,
    offset,
    vk::Extent3D{imageExtent.width, fullLines, 1},
    staging_offset);

  pixelsLeft -= fullLines * imageExtent.width;
  ETNA_ASSERT(finalOffset.x == int32_t(pixelsLeft));
  if (pixelsLeft == 0)
    return;

  staging_offset += fullLines * imageExtent.width * bytes_per_pixel;
  offset = skipLines(offset, fullLines);

  if (finalOffset.x > 0)
  {
    uploadImageRect(
      cmd_buf,
      dst,
      mip_level,
      layer,
      offset,
      vk::Extent3D{uint32_t(finalOffset.x), 1, 1},
      staging_offset);
  }
}

void PerFrameTransferHelper::uploadImageRect(
  vk::CommandBuffer cmd_buf,
  const Image& dst,
  uint32_t mip_level,
  uint32_t layer,
  vk::Offset3D offset,
  vk::Extent3D extent,
  vk::DeviceSize staging_offset)
{
  vk::BufferImageCopy2 copy{
    .bufferOffset = staging_offset,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource =
      vk::ImageSubresourceLayers{
        .aspectMask = dst.getAspectMaskByFormat(),
        .mipLevel = mip_level,
        .baseArrayLayer = layer,
        .layerCount = 1,
      },
    .imageOffset = offset,
    .imageExtent = extent,
  };
  vk::CopyBufferToImageInfo2 info{
    .srcBuffer = stagingBuffer.get().get(),
    .dstImage = dst.get(),
    .dstImageLayout = vk::ImageLayout::eTransferDstOptimal,
    .regionCount = 1,
    .pRegions = &copy,
  };
  cmd_buf.copyBufferToImage2(info);
}

} // namespace etna
