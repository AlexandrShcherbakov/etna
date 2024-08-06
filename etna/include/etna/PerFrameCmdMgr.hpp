#pragma once
#ifndef ETNA_PER_FRAME_CMD_MGR_HPP_INCLUDED
#define ETNA_PER_FRAME_CMD_MGR_HPP_INCLUDED

#include <optional>

#include <etna/Vulkan.hpp>
#include <etna/GpuWorkCount.hpp>
#include <etna/GpuSharedResource.hpp>


namespace etna
{

/**
 * Simple manager of command buffers. Provides a single command buffer per
 * frame and provides a simple API to submit it to the relevant queue every frame.
 * Suboptimal for many reasons, so feel free to read the implementation and be
 * inspired by it.
 */
class PerFrameCmdMgr
{
public:
  struct Dependencies
  {
    const GpuWorkCount& workCount;
    vk::Device device;

    vk::Queue submitQueue;
    std::uint32_t queueFamily;
  };

  explicit PerFrameCmdMgr(const Dependencies& deps);

  /**
   * Acquires the command buffer to use this frame, waiting for
   * the GPU to complete previous multi-buffering-count frames if necessary.
   */
  vk::CommandBuffer acquireNext();

  /**
   * Submits the command buffer acquired from acquire(), but allows it to
   * write to color attachments only after the `after` semaphore is signaled.
   * Intended to be used with a semaphore that signals availability of the
   * swap chain image used in the buffer (see etna::Window::acquireNext).
   * Returns a semaphore that is signaled when GPU has done executing the buffer,
   * which is intended to be used for presenting the swap chain image (see etna::Window::present).
   */
  vk::Semaphore submit(vk::CommandBuffer what, vk::Semaphore write_attachments_after);

private:
  vk::Device device;
  vk::Queue submitQueue;

  vk::UniqueCommandPool pool;
  GpuSharedResource<vk::UniqueFence> commandsComplete;
  GpuSharedResource<bool> commandsSubmitted;

  // NOTE: semaphores are GPU-only resources, no need to multi-buffer them.
  vk::UniqueSemaphore gpuDone;

  std::optional<GpuSharedResource<vk::UniqueCommandBuffer>> buffers;
};

} // namespace etna


#endif // ETNA_PER_FRAME_CMD_MGR_HPP_INCLUDED
