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
    VkPipelineStageFlags piplineStageFlags = VK_PIPELINE_STAGE_NONE;
    VkAccessFlags accessFlags = VK_ACCESS_NONE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkCommandBuffer owner = VK_NULL_HANDLE;
    bool operator==(const TextureState &other) const
    {
      return piplineStageFlags == other.piplineStageFlags && accessFlags == other.accessFlags && layout == other.layout && owner == other.owner;
    }
  };
  using State = std::variant<TextureState>; // TODO: Add buffers
  std::unordered_map<HandleType, State> currentStates;
  std::vector<VkImageMemoryBarrier2> barriersToFlush;
  template<VkPipelineStageFlagBits PipelineStageFlag, VkAccessFlags AccessFlags, VkImageLayout Layout, VkImageAspectFlags AspectFlags>
  void setRenderTargetBase(VkCommandBuffer com_buffer, VkImage image);
public:
  void setRenderTarget(VkCommandBuffer com_buffer, VkImage image);
  void setDepthRenderTarget(VkCommandBuffer com_buffer, VkImage image);
  void flushBarriers(VkCommandBuffer com_buf);
};

}

#endif // ETNA_STATETRACKING_HPP_INCLUDED
