#include <etna/PerFrameCmdMgr.hpp>

#include <tracy/Tracy.hpp>


namespace etna
{

PerFrameCmdMgr::PerFrameCmdMgr(const Dependencies &deps)
  : device{deps.device}
  , submitQueue{deps.submitQueue}
  , pool{unwrap_vk_result(deps.device.createCommandPoolUnique(vk::CommandPoolCreateInfo{
    .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
    .queueFamilyIndex = deps.queueFamily,
  }))}
  , commandsComplete{deps.workCount, [&deps](std::size_t){
    return unwrap_vk_result(deps.device.createFenceUnique(vk::FenceCreateInfo{}));
  }}
  , commandsSubmitted{deps.workCount, std::in_place, false}
{
  vk::CommandBufferAllocateInfo cbInfo{
    .commandPool = pool.get(),
    .level = vk::CommandBufferLevel::ePrimary,
    .commandBufferCount = static_cast<uint32_t>(deps.workCount.multiBufferingCount()),
  };
  auto bufsVec = unwrap_vk_result(deps.device.allocateCommandBuffersUnique(cbInfo));
  buffers.emplace(deps.workCount, [&bufsVec](std::size_t i) { return std::move(bufsVec[i]); });
}

vk::CommandBuffer PerFrameCmdMgr::acquireNext()
{
  ZoneScoped;

  if (!commandsSubmitted.get())
    return buffers->get().get();

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

  commandsSubmitted.get() = false;

  return curBuf;
}

vk::Semaphore PerFrameCmdMgr::submit(
  vk::CommandBuffer what, vk::Semaphore write_attachments_after, vk::Semaphore&& use_result_after)
{
  ZoneScoped;

  // NOTE: the only point in passing in `what` here is for
  // symmetry an aesthetic reasons.
  ETNA_VERIFY(what == buffers->get().get());
  std::array cbsInfo{vk::CommandBufferSubmitInfo{
    .commandBuffer = what,
    .deviceMask = 1,
  }};

  std::array wait{vk::SemaphoreSubmitInfo{
    .semaphore = write_attachments_after,
    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    .deviceIndex = 0,
  }};

  // NOTE: this is intended to be used for presenting, and as far
  // as I can tell, stageMask cannot do anything sensible on HW level
  // here. Also, there are outstanding issues about this in VK spec, so...
  // https://github.com/KhronosGroup/Vulkan-Docs/issues/1308
  std::array signal{vk::SemaphoreSubmitInfo{
    .semaphore = use_result_after,
    .stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    .deviceIndex = 0,
  }};

  vk::SubmitInfo2 sInfo{};
  sInfo.setCommandBufferInfos(cbsInfo);
  sInfo.setWaitSemaphoreInfos(wait);
  sInfo.setSignalSemaphoreInfos(signal);
  ETNA_CHECK_VK_RESULT(submitQueue.submit2({sInfo}, commandsComplete.get().get()));

  commandsSubmitted.get() = true;

  return use_result_after;
}

std::size_t PerFrameCmdMgr::getCmdBufferCount() const
{
  return buffers->getWorkCount()->multiBufferingCount();
}

} // namespace etna
