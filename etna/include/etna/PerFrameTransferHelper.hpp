#pragma once
#ifndef ETNA_PER_FRAME_TRANSFER_HELPER_HPP_INCLUDED
#define ETNA_PER_FRAME_TRANSFER_HELPER_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/Buffer.hpp>
#include <etna/Image.hpp>
#include <etna/GpuSharedResource.hpp>
#include <etna/EtnaConfig.hpp>

#include <type_traits>

/**
 * PerFrameTransferHelper : "non-blocking" GPU-CPU transfer helper,
 * allows to transfer resources frame by frame with a dedicated staging buffer.
 *
 * Is capable of both readbacks and uploads.
 * Uploads have a sync API (the whole resource will be uploaded this frame),
 * and both uploads and readbacks have async API. Async API works by the
 * user first calling init(action)(resource)Async to get back an
 * Async(resource)(action)State struct which holds the data about the progress.
 * Then, each frame the user calls progress(resource)(action)Async on the
 * state struct until the transfer has finished.
 *
 * (resource) may be "Buffer" or "Image"
 * (action) may be "Readback" for buffers or "Upload" for any resource type.
 *
 * Usage of PerFrameTransferHelper looks as follows.
 *
 * PerFrameTransferHelper th{...};
 *
 * -- init calls for async operations states can be called any time
 *
 * auto bufferUpload = th.initUploadBufferAsync(buf1, 0, bufData1);
 * auto bufferReadback = th.initReadbackBufferAsync(bufStorageDest2, buf2, 0);
 * auto imageUpload = th.initUploadImageAsync(img1, 0, 0, imgData1);
 *
 * -- actual sync/progress calls must be done every frame inside of processing
 * -- scope, for that the API is split into several classes to enforce order.
 *
 * if (auto frame = th.beginFrame())
 * {
 *   if (auto rb = frame.beginReadback())
 *   {
 *     if (!rb.hasSpaceThisFrame())
 *       ; -- Out of staging space, all upload/progress calls will do nothing
 *     if (rb.progressBufferReadbackAsync(cmd_buf, bufferReadback))
 *       ; -- Readback is done
 *   }
 *   if (auto up = frame.beginUpload())
 *   {
 *     if (up.progressBufferUploadAsync(cmd_buf, bufferUpload))
 *       ; -- Upload will be completed this frame
 *     if (up.progressImageUploadAsync(cmd_buf, imageUpload))
 *       ; -- Upload will be completed this frame
 *     if (up.uploadBufferSync(cmd_buf, buf3, 0, bufData3))
 *       ; -- There was space in staging and upload will be done
 *     if (up.uploadImageSync(cmd_buf, img2, 0, 0, imgData2))
 *       ; -- There was space in staging and upload will be done
 *
 *     for (auto &texUpload : texUploads)
 *     {
 *       if (!up.hasSpaceThisFrame())
 *         break;
 *       if (texUpload.done())
 *         continue;
 *       if (up.progressImageUploadAsync(cmd_buf, texUpload))
 *         ; -- Upload will be completed this frame
 *     }
 *   }
 * }
 *
 *
 * The order of calls and other constraints are enforced by asserts.
 * For example, all beginXXX calls can be called once per frame,
 * and readback can not be done after upload. Furthermore, if a portion
 * of a buffer readback was ready on frame N and progressXXX was not called
 * on that state, the content may be discarded and on the next call
 * the validation will err due to a missed piece of readback.
 *
 * In general one should progress all oustanding readbacks on every frame,
 * and uploads can be progressed however one wishes.
 */

namespace etna
{

class PerFrameTransferHelper;

struct AsyncBufferUploadState
{
  const PerFrameTransferHelper* transferHelper;
  uint64_t lastFrame;
  const Buffer* dst;
  uint32_t offset;
  std::span<std::byte const> src;

  bool done() const { return transferHelper != nullptr && src.empty(); }
};

struct AsyncBufferReadbackState
{
  struct Issue
  {
    uint64_t fulfillmentFrame = uint64_t(-1);
    uint32_t size = 0;
    uint32_t offset = 0;

    bool operator==(const Issue&) const = default;
  };

  const PerFrameTransferHelper* transferHelper;
  uint64_t lastFrame;
  const Buffer* src;
  uint32_t issuedOffset;
  uint32_t remainingOffset;
  std::array<Issue, MAX_FRAMES_INFLIGHT> issues;
  size_t nextIssueSlot;
  std::span<std::byte> dst;

  bool done() const { return transferHelper != nullptr && dst.empty(); }
};

struct AsyncImageUploadState
{
  const PerFrameTransferHelper* transferHelper;
  uint64_t lastFrame;
  const Image* dst;
  uint32_t mipLevel;
  uint32_t layer;
  size_t bytesPerPixel;
  vk::Offset3D offset;
  std::span<std::byte const> src;

  bool done() const { return transferHelper != nullptr && src.empty(); }
};

class PerFrameTransferHelper
{
private:
  enum class ProcessingState
  {
    IDLE,
    READY,
    READBACK,
    READBACK_DONE,
    UPLOAD,
    UPLOAD_DONE,
  };

  class FrameProcessor;

  class UploadProcessor
  {
    PerFrameTransferHelper* owner;
    friend class FrameProcessor;

  public:
    explicit UploadProcessor(PerFrameTransferHelper* owner)
      : owner{owner}
    {
    }

    UploadProcessor(const UploadProcessor&) = delete;
    UploadProcessor& operator=(const UploadProcessor&) = delete;
    UploadProcessor(UploadProcessor&&) = delete;
    UploadProcessor& operator=(UploadProcessor&&) = delete;

    ~UploadProcessor() { finish(); }

    template <class T>
      requires std::is_trivially_copyable_v<T>
    bool uploadBufferSync(
      vk::CommandBuffer cmd_buf, const Buffer& dst, uint32_t offset, std::span<T const> src)
    {
      std::span<std::byte const> raw{
        reinterpret_cast<const std::byte*>(src.data()), src.size_bytes()};
      return uploadBufferSync(cmd_buf, dst, offset, raw);
    }

    bool uploadBufferSync(
      vk::CommandBuffer cmd_buf, const Buffer& dst, uint32_t offset, std::span<std::byte const> src)
    {
      return owner->uploadBufferSync(cmd_buf, dst, offset, src);
    }

    bool uploadImageSync(
      vk::CommandBuffer cmd_buf,
      const Image& dst,
      uint32_t mip_level,
      uint32_t layer,
      std::span<std::byte const> src)
    {
      return owner->uploadImageSync(cmd_buf, dst, mip_level, layer, src);
    }

    bool progressBufferUploadAsync(vk::CommandBuffer cmd_buf, AsyncBufferUploadState& state)
    {
      return owner->progressBufferUploadAsync(cmd_buf, state);
    }
    bool progressImageUploadAsync(vk::CommandBuffer cmd_buf, AsyncImageUploadState& state)
    {
      return owner->progressImageUploadAsync(cmd_buf, state);
    }

    bool hasSpaceThisFrame() const { return owner->curFrameStagingOffset < owner->stagingSize; }

    void finish();

    explicit operator bool() const { return owner != nullptr; }
  };

  class ReadbackProcessor
  {
    PerFrameTransferHelper* owner;
    friend class FrameProcessor;

  public:
    explicit ReadbackProcessor(PerFrameTransferHelper* owner)
      : owner{owner}
    {
    }

    ReadbackProcessor(const ReadbackProcessor&) = delete;
    ReadbackProcessor& operator=(const ReadbackProcessor&) = delete;
    ReadbackProcessor(ReadbackProcessor&&) = delete;
    ReadbackProcessor& operator=(ReadbackProcessor&&) = delete;

    ~ReadbackProcessor() { finish(); }

    bool progressBufferReadbackAsync(vk::CommandBuffer cmd_buf, AsyncBufferReadbackState& state)
    {
      return owner->progressBufferReadbackAsync(cmd_buf, state);
    }

    bool hasSpaceThisFrame() const { return owner->curFrameStagingOffset < owner->stagingSize; }

    void finish();

    explicit operator bool() const { return owner != nullptr; }
  };

  class FrameProcessor
  {
    PerFrameTransferHelper* owner;
    friend class PerFrameTransferHelper;

  public:
    explicit FrameProcessor(PerFrameTransferHelper* owner)
      : owner{owner}
    {
    }

    FrameProcessor(const FrameProcessor&) = delete;
    FrameProcessor& operator=(const FrameProcessor&) = delete;
    FrameProcessor(FrameProcessor&&) = delete;
    FrameProcessor& operator=(FrameProcessor&&) = delete;

    ~FrameProcessor() { finish(); }

    ReadbackProcessor beginReadback();
    UploadProcessor beginUpload();

    void finish();

    explicit operator bool() const { return owner != nullptr; }
  };

  friend class UploadProcessor;
  friend class ReadbackProcessor;
  friend class FrameProcessor;

public:
  struct CreateInfo
  {
    vk::DeviceSize totalStagingSize;
    const GpuWorkCount* wc;
  };

  explicit PerFrameTransferHelper(CreateInfo info);

  PerFrameTransferHelper(const PerFrameTransferHelper&) = delete;
  PerFrameTransferHelper& operator=(const PerFrameTransferHelper&) = delete;
  PerFrameTransferHelper(PerFrameTransferHelper&&) = delete;
  PerFrameTransferHelper& operator=(PerFrameTransferHelper&&) = delete;

  FrameProcessor beginFrame();

  template <class T>
    requires std::is_trivially_copyable_v<T>
  AsyncBufferUploadState initUploadBufferAsync(
    const Buffer& dst, uint32_t offset, std::span<T const> src) const
  {
    std::span<std::byte const> raw{
      reinterpret_cast<const std::byte*>(src.data()), src.size_bytes()};
    return initUploadBufferAsync(dst, offset, raw);
  }

  AsyncBufferUploadState initUploadBufferAsync(
    const Buffer& dst, uint32_t offset, std::span<std::byte const> src) const;

  template <class T>
    requires std::is_trivially_copyable_v<T>
  AsyncBufferReadbackState initReadbackBufferAsync(
    std::span<T> dst, const Buffer& src, uint32_t offset) const
  {
    std::span<std::byte> raw{reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes()};
    return initReadbackBufferAsync(raw, src, offset);
  }

  AsyncBufferReadbackState initReadbackBufferAsync(
    std::span<std::byte> dst, const Buffer& src, uint32_t offset) const;

  // @NOTE: for now doesn't support 3D images (unlike sync API)
  AsyncImageUploadState initUploadImageAsync(
    const Image& dst, uint32_t mip_level, uint32_t layer, std::span<std::byte const> src) const;

private:
  ProcessingState inFrameState;
  uint64_t lastFrame;
  vk::DeviceSize stagingSize;
  vk::DeviceSize curFrameStagingOffset;
  GpuSharedResource<Buffer> stagingBuffer;
  const GpuWorkCount& wc;

  bool uploadBufferSync(
    vk::CommandBuffer cmd_buf, const Buffer& dst, uint32_t offset, std::span<std::byte const> src);

  bool uploadImageSync(
    vk::CommandBuffer cmd_buf,
    const Image& dst,
    uint32_t mip_level,
    uint32_t layer,
    std::span<std::byte const> src);

  bool progressBufferUploadAsync(vk::CommandBuffer cmd_buf, AsyncBufferUploadState& state);
  bool progressBufferReadbackAsync(vk::CommandBuffer cmd_buf, AsyncBufferReadbackState& state);
  bool progressImageUploadAsync(vk::CommandBuffer cmd_buf, AsyncImageUploadState& state);

  void transferBufferRegion(
    vk::CommandBuffer cmd_buf,
    const Buffer& src,
    const Buffer& dst,
    vk::DeviceSize src_offset,
    vk::DeviceSize dst_offset,
    size_t size);

  void uploadImageRegion(
    vk::CommandBuffer cmd_buf,
    const Image& dst,
    uint32_t mip_level,
    uint32_t layer,
    size_t bytes_per_pixel,
    vk::Offset3D offset,
    vk::DeviceSize staging_offset,
    size_t size);

  void uploadImageRect(
    vk::CommandBuffer cmd_buf,
    const Image& dst,
    uint32_t mip_level,
    uint32_t layer,
    vk::Offset3D offset,
    vk::Extent3D extent,
    vk::DeviceSize staging_offset);
};

} // namespace etna

#endif
