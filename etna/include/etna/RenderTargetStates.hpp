#pragma once
#ifndef ETNA_STATES_HPP_INCLUDED
#define ETNA_STATES_HPP_INCLUDED

#include <vector>

#include <etna/Vulkan.hpp>
#include <etna/Image.hpp>


namespace etna
{

class RenderTargetState
{
  VkCommandBuffer commandBuffer;
  static bool inScope;

public:
  struct AttachmentParams
  {
    vk::Image image = VK_NULL_HANDLE;
    vk::ImageView view = VK_NULL_HANDLE;
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearColorValue clearColorValue = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
    vk::ClearDepthStencilValue clearDepthStencilValue = {1.0f, 0};

    // By default, the render target can work with multisample images and pipelines,
    // but not produce a final single-sample result.
    // These fields below are for the final MSAA image.
    // Ignore unless you know what MSAA is and aren't sure you need it.
    vk::Image resolveImage = VK_NULL_HANDLE;
    vk::ImageView resolveImageView = VK_NULL_HANDLE;
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;
  };

  RenderTargetState(
    VkCommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment,
    AttachmentParams stencil_attachment);

  // We can't use the default argument for stencil_attachment due to gcc bug 88165
  // See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88165
  RenderTargetState(
    VkCommandBuffer cmd_buff,
    vk::Rect2D rect,
    const std::vector<AttachmentParams>& color_attachments,
    AttachmentParams depth_attachment)
    : RenderTargetState(cmd_buff, rect, color_attachments, depth_attachment, {}){};

  ~RenderTargetState();
};

} // namespace etna

#endif // ETNA_STATES_HPP_INCLUDED
