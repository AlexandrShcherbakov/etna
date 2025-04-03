#include "StateTracking.hpp"
#include "etna/GlobalContext.hpp"

#include <bit>


namespace etna
{

void ResourceStates::setExternalTextureState(
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkImage>(image));
  currentStates[resHandle] = TextureState{
    .piplineStageFlags = pipeline_stage_flag,
    .accessFlags = access_flags,
    .layout = layout,
    .owner = {},
  };
}

void ResourceStates::setTextureState(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags,
  ForceSetState force)
{
  HandleType resHandle = std::bit_cast<HandleType>(static_cast<VkImage>(image));
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
  if (force == ForceSetState::eFalse && newState == oldState)
    return;
  barriersToFlush.push_back(vk::ImageMemoryBarrier2{
    .srcStageMask = oldState.piplineStageFlags,
    .srcAccessMask = oldState.accessFlags,
    .dstStageMask = newState.piplineStageFlags,
    .dstAccessMask = newState.accessFlags,
    .oldLayout = oldState.layout,
    .newLayout = newState.layout,
    .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
    .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
    .image = image,
    .subresourceRange =
      {
        .aspectMask = aspect_flags,
        .baseMipLevel = 0,
        .levelCount = vk::RemainingMipLevels,
        .baseArrayLayer = 0,
        .layerCount = vk::RemainingArrayLayers,
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
  com_buf.pipelineBarrier2(depInfo);
  barriersToFlush.clear();
}

void ResourceStates::setColorTarget(
  vk::CommandBuffer com_buffer, vk::Image image, BarrierBehavior behavior)
{
  if (get_context().shouldGenerateBarriersWhen(behavior))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput,
      vk::AccessFlagBits2::eColorAttachmentWrite,
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ImageAspectFlagBits::eColor);
  }
}

void ResourceStates::setDepthStencilTarget(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::ImageAspectFlags aspect_flags,
  BarrierBehavior behavior)
{
  if (get_context().shouldGenerateBarriersWhen(behavior))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eEarlyFragmentTests |
        vk::PipelineStageFlagBits2::eLateFragmentTests,
      vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
      vk::ImageLayout::eDepthStencilAttachmentOptimal,
      aspect_flags);
  }
}

void ResourceStates::setResolveTarget(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::ImageAspectFlags aspect_flags,
  BarrierBehavior behavior)
{
  if (get_context().shouldGenerateBarriersWhen(behavior))
  {
    setTextureState(
      com_buffer,
      image,
      vk::PipelineStageFlagBits2::eResolve,
      vk::AccessFlagBits2::eTransferWrite,
      vk::ImageLayout::eGeneral,
      aspect_flags);
  }
}

} // namespace etna
