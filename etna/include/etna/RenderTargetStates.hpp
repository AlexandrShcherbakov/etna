#pragma once
#ifndef ETNA_STATES_HPP_INCLUDED
#define ETNA_STATES_HPP_INCLUDED

#include <etna/Image.hpp>

#include <vulkan/vulkan.hpp>

#include <vector>

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
        AttachmentParams() = default;
        AttachmentParams(vk::Image i, vk::ImageView v, bool clear = true)
          : image(i), view(v), loadOp(clear ? vk::AttachmentLoadOp::eClear : vk::AttachmentLoadOp::eLoad) {}
        AttachmentParams(const Image &img, bool clear = true) : AttachmentParams(img.get(), img.getView({}), clear) {}
    };
    
    RenderTargetState(VkCommandBuffer cmd_buff, vk::Rect2D rect,
    const std::vector<AttachmentParams> &color_attachments, AttachmentParams depth_attachment);

    ~RenderTargetState();
};

}

#endif // ETNA_STATES_HPP_INCLUDED
