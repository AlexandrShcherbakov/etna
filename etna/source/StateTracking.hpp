#pragma once
#ifndef ETNA_STATETRACKING_HPP_INCLUDED
#define ETNA_STATETRACKING_HPP_INCLUDED

#include "etna/Vulkan.hpp"
#include "etna/BarrierBehavior.hpp"

#include <variant>
#include <unordered_map>

namespace etna
{

class ResourceStates
{
  using HandleType = uint64_t;
  struct TextureState
  {
    vk::PipelineStageFlags2 piplineStageFlags = {};
    vk::AccessFlags2 accessFlags = {};
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::CommandBuffer owner = {};
    bool operator==(const TextureState& other) const = default;
  };
  using State = std::variant<TextureState>; // TODO: Add buffers
  std::unordered_map<HandleType, State> currentStates;
  std::vector<vk::ImageMemoryBarrier2> barriersToFlush;

public:
  void setExternalTextureState(
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout);

  void setTextureState(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::PipelineStageFlags2 pipeline_stage_flag,
    vk::AccessFlags2 access_flags,
    vk::ImageLayout layout,
    vk::ImageAspectFlags aspect_flags,
    ForceSetState force = ForceSetState::eFalse);

  void setColorTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    BarrierBehavior behavior = BarrierBehavior::eDefault);
  void setDepthStencilTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavior behavior = BarrierBehavior::eDefault);
  void setResolveTarget(
    vk::CommandBuffer com_buffer,
    vk::Image image,
    vk::ImageAspectFlags aspect_flags,
    BarrierBehavior behavior = BarrierBehavior::eDefault);

  void flushBarriers(vk::CommandBuffer com_buf);
};

} // namespace etna

#endif // ETNA_STATETRACKING_HPP_INCLUDED
