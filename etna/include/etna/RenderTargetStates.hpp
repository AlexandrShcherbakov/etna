#pragma once
#ifndef ETNA_STATES_HPP_INCLUDED
#define ETNA_STATES_HPP_INCLUDED

#include <vulkan/vulkan.hpp>

#include <vector>

namespace etna
{

class RenderTargetState
{
    VkCommandBuffer commandBuffer;
public:
    struct AttachmentParams
    {
        // TODO: Add new fields for clearing etc.
        vk::ImageView view = VK_NULL_HANDLE;
        AttachmentParams() = default;
        AttachmentParams(vk::ImageView v) : view(v) {}
    };
    
    RenderTargetState(VkCommandBuffer cmd_buff, vk::Extent2D extend,
    const std::vector<AttachmentParams> &color_attachments, AttachmentParams depth_attachment);
    ~RenderTargetState();
};

}

#endif // ETNA_STATES_HPP_INCLUDED
