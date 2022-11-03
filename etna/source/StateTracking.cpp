#include "StateTracking.hpp"

namespace etna
{

void ResourceStates::setTextureState(VkCommandBuffer com_buffer, VkImage image,
  vk::PipelineStageFlagBits2 pipeline_stage_flag, vk::AccessFlags2 access_flags,
  vk::ImageLayout layout, vk::ImageAspectFlags aspect_flags)
{
  HandleType resHandle = reinterpret_cast<HandleType>(image);
  if (currentStates.count(resHandle) == 0)
  {
    currentStates[resHandle] = TextureState{
      .owner = com_buffer
    };
  }
  TextureState newState {
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .layout = layout,
    .owner = com_buffer
  };
  auto &oldState = std::get<0>(currentStates[resHandle]);
  if (newState == oldState)
    return;
  barriersToFlush.push_back(
    vk::ImageMemoryBarrier2
    {
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
        .aspectMask = aspect_flags,
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
  vk::DependencyInfo depInfo
  {
    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    .imageMemoryBarrierCount = static_cast<uint32_t>(barriersToFlush.size()),
    .pImageMemoryBarriers = barriersToFlush.data(),
  };
  VkDependencyInfo depInformation = depInfo;
  vkCmdPipelineBarrier2(com_buf, &depInformation);
  barriersToFlush.clear();
}

void ResourceStates::setRenderTarget(VkCommandBuffer com_buffer, VkImage image)
{
  setTextureState(com_buffer, image,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);
}

void ResourceStates::setDepthRenderTarget(VkCommandBuffer com_buffer, VkImage image)
{
  setTextureState(com_buffer, image,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::ImageLayout::eDepthStencilAttachmentOptimal,
    vk::ImageAspectFlagBits::eDepth);
}

}
