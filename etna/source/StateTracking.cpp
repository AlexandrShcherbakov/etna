#include "StateTracking.hpp"

namespace etna
{

template<VkPipelineStageFlagBits PipelineStageFlag, VkAccessFlags AccessFlags, VkImageLayout Layout, VkImageAspectFlags AspectFlags>
void ResourceStates::setRenderTargetBase(VkCommandBuffer com_buffer, VkImage image)
{
  HandleType resHandle = reinterpret_cast<HandleType>(image);
  if (currentStates.count(resHandle) == 0)
  {
    currentStates[resHandle] = TextureState{
      .owner = com_buffer
    };
  }
  TextureState newState {
    .piplineStageFlags = PipelineStageFlag,
    .accessFlags = AccessFlags,
    .layout = Layout,
    .owner = com_buffer
  };
  auto &oldState = std::get<0>(currentStates[resHandle]);
  if (newState == oldState)
    return;
  barriersToFlush.push_back(
    VkImageMemoryBarrier2
    {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
      .pNext = nullptr,
      .srcStageMask = oldState.piplineStageFlags,
      .srcAccessMask = oldState.accessFlags,
      .dstStageMask = newState.piplineStageFlags,
      .dstAccessMask = newState.accessFlags,
      .oldLayout = oldState.layout,
      .newLayout = newState.layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
      {
        .aspectMask = AspectFlags,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
      }
    }
  );
  oldState = newState;
}

void ResourceStates::flushBarriers(VkCommandBuffer com_buf)
{
  if (barriersToFlush.empty())
    return;
  VkDependencyInfo depInfo
  {
    .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
    .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    .imageMemoryBarrierCount = static_cast<uint32_t>(barriersToFlush.size()),
    .pImageMemoryBarriers = barriersToFlush.data(),
  };
  vkCmdPipelineBarrier2(com_buf, &depInfo);
  barriersToFlush.clear();
}

void ResourceStates::setRenderTarget(VkCommandBuffer com_buffer, VkImage image)
{
  setRenderTargetBase<
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_COLOR_BIT
  >(com_buffer, image);
}

void ResourceStates::setDepthRenderTarget(VkCommandBuffer com_buffer, VkImage image)
{
  setRenderTargetBase<
    VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_ASPECT_DEPTH_BIT
  >(com_buffer, image);
}

}
