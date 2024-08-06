#include <etna/OneShotCmdMgr.hpp>

namespace etna
{

OneShotCmdMgr::OneShotCmdMgr(const Dependencies& deps)
  : device{deps.device}
  , submitQueue{deps.submitQueue}
  , pool{unwrap_vk_result(deps.device.createCommandPoolUnique(vk::CommandPoolCreateInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = deps.queueFamily,
    }))}
  , commandBuffer{std::move(
      unwrap_vk_result(deps.device.allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
        .commandPool = pool.get(),
        .level = vk::CommandBufferLevel::ePrimary,
        .commandBufferCount = 1,
      }))[0])}
  , oneShotFinished{unwrap_vk_result(deps.device.createFenceUnique(vk::FenceCreateInfo{}))}
{
}

vk::CommandBuffer OneShotCmdMgr::start()
{
  return commandBuffer.get();
}

void OneShotCmdMgr::submitAndWait(vk::CommandBuffer buffer)
{
  ETNA_ASSERT(buffer == commandBuffer.get());

  std::array cbsInfo{vk::CommandBufferSubmitInfo{
    .commandBuffer = commandBuffer.get(),
    .deviceMask = 1,
  }};

  vk::SubmitInfo2 sInfo{};
  sInfo.setCommandBufferInfos(cbsInfo);
  ETNA_CHECK_VK_RESULT(submitQueue.submit2({sInfo}, oneShotFinished.get()));
  ETNA_CHECK_VK_RESULT(device.waitForFences({oneShotFinished.get()}, vk::True, 1000000000));

  device.resetFences({oneShotFinished.get()});
  commandBuffer->reset();
}

} // namespace etna
