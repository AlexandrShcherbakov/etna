#include <etna/CommandManager.hpp>

namespace etna
{

CommandManager::CommandManager(const Dependencies &deps)
  : device{deps.device}
  , submitQueue{deps.submitQueue}
  , pool{unwrap_vk_result(deps.device.createCommandPoolUnique(vk::CommandPoolCreateInfo{
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = deps.queueFamily,
  }))}
  , commandsComplete{deps.workCount, [&deps](std::size_t){
    return unwrap_vk_result(deps.device.createFenceUnique(vk::FenceCreateInfo{ .flags = vk::FenceCreateFlagBits::eSignaled }));
  }}
  , gpuDone{unwrap_vk_result(deps.device.createSemaphoreUnique({}))}
{
  vk::CommandBufferAllocateInfo cbInfo{
    .commandPool = pool.get(),
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = static_cast<uint32_t>(deps.workCount.multiBufferingCount()),
  };
  auto bufsVec = unwrap_vk_result(deps.device.allocateCommandBuffersUnique(cbInfo));
  buffers.emplace(deps.workCount, [&bufsVec](std::size_t i) { return std::move(bufsVec[i]); });
}

vk::CommandBuffer CommandManager::acquireNext()
{
  // Wait for previous execution of the current command
  // buffer to complete. It may in fact already be long
  // finished, but we still have to synchronize GPU and CPU
  // explicitly. This also synchronizes all other shared
  // resources which live inside GpuSharedResource containers.
  auto curComplete = commandsComplete.get().get();
  ETNA_CHECK_VK_RESULT(device.waitForFences({curComplete}, vk::True, 1000000000));
  device.resetFences({curComplete});

  auto curBuf = buffers->get().get();
  curBuf.reset();

  return curBuf;
}

vk::Semaphore CommandManager::submit(vk::CommandBuffer what, vk::Semaphore write_attachments_after)
{
  // NOTE: the only point in passing in `what` here is for
  // symmetry an aesthetic reasons.
  ETNA_ASSERT(what == buffers->get().get());
  std::array cbsInfo{vk::CommandBufferSubmitInfo{
    .commandBuffer = what,
    .deviceMask = 1,
  }};

  std::array wait{vk::SemaphoreSubmitInfo{
    .semaphore = write_attachments_after,
    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    .deviceIndex = 1,
  }};

  // NOTE: this is intended to be used for presenting, and as far
  // as I can tell, stageMask cannot do anything sensible on HW level
  // here. Also, there are outstanding issues about this in VK spec, so...
  // https://github.com/KhronosGroup/Vulkan-Docs/issues/1308
  std::array signal{vk::SemaphoreSubmitInfo{
    .semaphore = gpuDone.get(),
    .stageMask = {},
    .deviceIndex = 1,
  }};

  vk::SubmitInfo2 sInfo{};
  sInfo.setCommandBufferInfos(cbsInfo);
  sInfo.setWaitSemaphoreInfos(wait);
  sInfo.setSignalSemaphoreInfos(signal);
  ETNA_CHECK_VK_RESULT(submitQueue.submit2({sInfo}, commandsComplete.get().get()));

  return gpuDone.get();
}

} // namespace etna
