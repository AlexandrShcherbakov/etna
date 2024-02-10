#include "etna/RenderTargetStates.hpp"

#include "etna/GlobalContext.hpp"
#include "StateTracking.hpp"

#include <unordered_map>
#include <variant>

namespace etna
{

bool RenderTargetState::inScope = false;

RenderTargetState::RenderTargetState(
  VkCommandBuffer cmd_buff,
  vk::Extent2D extend,
  const std::vector<AttachmentParams> &color_attachments,
  AttachmentParams depth_attachment,
  AttachmentParams stencil_attachment)
{
  ETNA_ASSERTF(!inScope, "RenderTargetState scopes shouldn't overlap.");
  inScope = true;
  // TODO: add resource state tracking
  commandBuffer = cmd_buff;
  vk::Viewport viewport
  {
    .x = static_cast<float>(rect.offset.x),
    .y = static_cast<float>(rect.offset.y),
    .width  = static_cast<float>(rect.extent.width),
    .height = static_cast<float>(rect.extent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f
  };

  VkViewport vp = (VkViewport)viewport;
  VkRect2D scis = (VkRect2D)rect;
  vkCmdSetViewport(commandBuffer, 0, 1, &vp);
  vkCmdSetScissor(commandBuffer, 0, 1, &scis);

  std::vector<vk::RenderingAttachmentInfo> attachmentInfos(color_attachments.size());
  for (uint32_t i = 0; i < color_attachments.size(); ++i)
  {
    attachmentInfos[i].imageView = color_attachments[i].view;
    attachmentInfos[i].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfos[i].loadOp = color_attachments[i].loadOp;
    attachmentInfos[i].storeOp = color_attachments[i].storeOp;
    attachmentInfos[i].clearValue = color_attachments[i].clearColorValue;
    etna::get_context().getResourceTracker().setColorTarget(commandBuffer, color_attachments[i].image);
  }

  vk::RenderingAttachmentInfo depthAttInfo {
    .imageView = depth_attachment.view,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .loadOp = depth_attachment.loadOp,
    .storeOp = depth_attachment.storeOp,
    .clearValue = depth_attachment.clearDepthStencilValue
  };
  
  vk::RenderingAttachmentInfo stencilAttInfo {
    .imageView = stencil_attachment.view,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .loadOp = stencil_attachment.loadOp,
    .storeOp = stencil_attachment.storeOp,
    .clearValue = stencil_attachment.clearDepthStencilValue
  };

  if (depth_attachment.image && stencil_attachment.image)
  {
    ETNA_ASSERTF(depth_attachment.view == stencil_attachment.view, "depth and stencil attachments must be created from the same image");
    etna::get_context().getResourceTracker().setDepthStencilTarget(commandBuffer, depth_attachment.image);
  }
  else
  {
    if (depth_attachment.image)
      etna::get_context().getResourceTracker().setDepthTarget(commandBuffer, depth_attachment.image);
    if (stencil_attachment.image)
      etna::get_context().getResourceTracker().setStencilTarget(commandBuffer, stencil_attachment.image);
  }

  etna::get_context().getResourceTracker().flushBarriers(commandBuffer);

  vk::RenderingInfo renderInfo {
    .renderArea = rect,
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size()),
    .pColorAttachments = attachmentInfos.size() ? attachmentInfos.data() : nullptr,
    .pDepthAttachment = depth_attachment.view ? &depthAttInfo : nullptr,
    .pStencilAttachment = stencil_attachment.view ? &stencilAttInfo : nullptr,
  };
  VkRenderingInfo rInf = (VkRenderingInfo)renderInfo;
  vkCmdBeginRendering(commandBuffer, &rInf);
}

RenderTargetState::~RenderTargetState()
{
  vkCmdEndRendering(commandBuffer);
  inScope = false;
}
}
