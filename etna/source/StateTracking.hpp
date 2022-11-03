#pragma once
#ifndef ETNA_STATETRACKING_HPP_INCLUDED
#define ETNA_STATETRACKING_HPP_INCLUDED

#include <vulkan/vulkan.hpp>

#include <variant>
#include <unordered_map>

namespace etna
{

class ResourceStates
{
  using HandleType = uint64_t;
  struct TextureState
  {
    vk::PipelineStageFlags2 piplineStageFlags = vk::PipelineStageFlags2();
    vk::AccessFlags2 accessFlags = vk::AccessFlags2();
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    VkCommandBuffer owner = VK_NULL_HANDLE;
    bool operator==(const TextureState &other) const
    {
      return piplineStageFlags == other.piplineStageFlags && accessFlags == other.accessFlags && layout == other.layout && owner == other.owner;
    }
  };
  using State = std::variant<TextureState>; // TODO: Add buffers
  std::unordered_map<HandleType, State> currentStates;
  std::vector<vk::ImageMemoryBarrier2> barriersToFlush;
public:
  void setTextureState(VkCommandBuffer com_buffer, VkImage image,
    vk::PipelineStageFlagBits2 pipeline_stage_flag, vk::AccessFlags2 access_flags,
    vk::ImageLayout layout, vk::ImageAspectFlags aspect_flags);

  void setRenderTarget(VkCommandBuffer com_buffer, VkImage image);
  void setDepthRenderTarget(VkCommandBuffer com_buffer, VkImage image);

  void flushBarriers(VkCommandBuffer com_buf);
};

}

#endif // ETNA_STATETRACKING_HPP_INCLUDED
