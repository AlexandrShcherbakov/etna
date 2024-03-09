#include "StateTracking.hpp"


namespace etna
{

void ResourceStates::setTextureState(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlagBits2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags)
{
  HandleType resHandle = reinterpret_cast<HandleType>((VkImage)image);
  if (currentStates.count(resHandle) == 0)
  {
    currentStates[resHandle] = TextureState{.owner = com_buffer};
  }
  TextureState newState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .layout = layout,
    .owner = com_buffer,
  };
  auto& oldState = std::get<0>(currentStates[resHandle]);
  if (newState == oldState)
    return;
  barriersToFlush.push_back(vk::ImageMemoryBarrier2{
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
      },
  });
  oldState = newState;
}

void ResourceStates::flushBarriers(vk::CommandBuffer com_buf)
{
  if (barriersToFlush.empty())
    return;
  vk::DependencyInfo depInfo{
    .dependencyFlags = vk::DependencyFlagBits::eByRegion,
    .imageMemoryBarrierCount = static_cast<uint32_t>(barriersToFlush.size()),
    .pImageMemoryBarriers = barriersToFlush.data(),
  };
  VkDependencyInfo depInformation = (VkDependencyInfo)depInfo;
  vkCmdPipelineBarrier2(com_buf, &depInformation);
  barriersToFlush.clear();
}

void ResourceStates::setColorTarget(vk::CommandBuffer com_buffer, vk::Image image)
{
  setTextureState(
    com_buffer,
    image,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eColorAttachmentWrite,
    vk::ImageLayout::eColorAttachmentOptimal,
    vk::ImageAspectFlagBits::eColor);
}

void ResourceStates::setDepthStencilTarget(vk::CommandBuffer com_buffer, vk::Image image)
{
  setTextureState(
    com_buffer,
    image,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::ImageLayout::eDepthStencilAttachmentOptimal,
    vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil);
}

void ResourceStates::setDepthTarget(vk::CommandBuffer com_buffer, vk::Image image)
{
  setTextureState(
    com_buffer,
    image,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::ImageLayout::eDepthStencilAttachmentOptimal,
    vk::ImageAspectFlagBits::eDepth);
}

void ResourceStates::setStencilTarget(vk::CommandBuffer com_buffer, vk::Image image)
{
  setTextureState(
    com_buffer,
    image,
    vk::PipelineStageFlagBits2::eEarlyFragmentTests,
    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
    vk::ImageLayout::eDepthStencilAttachmentOptimal,
    vk::ImageAspectFlagBits::eStencil);
}

void ResourceStates::setResolveTarget(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::ImageAspectFlags aspect_flags)
{
  setTextureState(
    com_buffer,
    image,
    vk::PipelineStageFlagBits2::eResolve,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eGeneral,
    aspect_flags);
}

} // namespace etna
