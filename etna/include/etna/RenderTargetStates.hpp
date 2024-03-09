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
    // TODO: Add new fields for clearing etc.
    vk::Image image = VK_NULL_HANDLE;
    vk::ImageView view = VK_NULL_HANDLE;
    vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear;
    vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore;
    vk::ClearColorValue clearColorValue = std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f});
    vk::ClearDepthStencilValue clearDepthStencilValue = {1.0f, 0};
    
    // resolve part
    vk::Image resolveImage = VK_NULL_HANDLE;
    vk::ImageView resolveImageView = VK_NULL_HANDLE;
    vk::ResolveModeFlagBits resolveMode = vk::ResolveModeFlagBits::eNone;

    AttachmentParams() = default;
    AttachmentParams(vk::Image i, vk::ImageView v, bool clear = true)
      : image(i)
      , view(v)
      , loadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad)
    {
    }
    explicit AttachmentParams(const Image& img, bool clear = true)
      : AttachmentParams(img.get(), img.getView({}), clear)
    {
    }
    AttachmentParams(vk::Image i, vk::ImageView v, vk::Image ri, vk::ImageView rv, bool clear = true, bool store = false)
      : image(i)
      , view(v)
      , loadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad)
      , storeOp(store ? vk::AttachmentStoreOp::eStore : vk::AttachmentStoreOp::eDontCare)
      , resolveImage(ri)
      , resolveImageView(rv)
      , resolveMode(vk::ResolveModeFlagBits::eAverage) 
    {
    }
    AttachmentParams(const Image &img, const Image &res_img, bool clear = true, bool store = false) 
      : AttachmentParams(img.get(), img.getView({}), res_img.get(), res_img.getView({}), clear, store)
    {
    }
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
