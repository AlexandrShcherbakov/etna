#pragma once
#ifndef ETNA_STATES_HPP_INCLUDED
#define ETNA_STATES_HPP_INCLUDED

#include <vector>

#include <etna/Vulkan.hpp>
#include <etna/Image.hpp>
#include <etna/BarrierBehavoir.hpp>

namespace etna
{

class RenderTargetState
{
  vk::CommandBuffer commandBuffer;
  static bool inScope;

public:
  struct AttachmentParams
  {
    vk::Image image = {};
    vk::ImageView view = {};
    std::optional<vk::ImageAspectFlags> imageAspect{};
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearColorValue clearColorValue = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
    vk::ClearDepthStencilValue clearDepthStencilValue = {1.0f, 0};

    // By default, the render target can work with multisample images and pipelines,
    // but not produce a final single-sample result.
    // These fields below are for the final MSAA image.
    // Ignore unless you know what MSAA is and aren't sure you need it.
    vk::Image resolveImage = {};
    vk::ImageView resolveImageView = {};
    std::optional<vk::ImageAspectFlags> resolveImageAspect{};
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
  };

  RenderTargetState(
    vk::CommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment,
    AttachmentParams stencil_attachment,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault);

  // We can't use the default argument for stencil_attachment due to gcc bug 88165
  // See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165
  RenderTargetState(
    vk::CommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment,
    BarrierBehavoir behavoir = BarrierBehavoir::eDefault)
    : RenderTargetState(cmd_buff, rect, color_attachments, depth_attachment, {}, behavoir)
  {
  }

  ~RenderTargetState();
};

} // namespace etna

#endif // ETNA_STATES_HPP_INCLUDED
