#include "etna/RenderTargetStates.hpp"

#include <unordered_map>
#include <variant>

namespace etna
{

RenderTargetState::RenderTargetState(
  VkCommandBuffer cmd_buff,
  vk::Extent2D extend,
  const std::vector<AttachmentParams> &color_attachments,
  AttachmentParams depth_attachment)
{
  // TODO: add resource state tracking
  commandBuffer = cmd_buff;
  vk::Viewport viewport
  {
    .x = 0.0f,
    .y = 0.0f,
    .width  = static_cast<float>(extend.width),
    .height = static_cast<float>(extend.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f
  };
  vk::Rect2D scissor
  {
    .offset = {0, 0},
    .extent = extend
  };

  VkViewport vp = (VkViewport)viewport;
  VkRect2D scis = (VkRect2D)scissor;
  vkCmdSetViewport(commandBuffer, 0, 1, &vp);
  vkCmdSetScissor(commandBuffer, 0, 1, &scis);

  std::vector<vk::RenderingAttachmentInfo> attachmentInfos(color_attachments.size());
  for (uint32_t i = 0; i < color_attachments.size(); ++i)
  {
  attachmentInfos[i].imageView = color_attachments[i].view;
    attachmentInfos[i].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfos[i].loadOp = vk::AttachmentLoadOp::eClear;
    attachmentInfos[i].storeOp = vk::AttachmentStoreOp::eStore;
    attachmentInfos[i].clearValue = vk::ClearColorValue{std::array<float, 4>({0.0f, 0.0f, 0.0f, 1.0f})};
  }
  vk::RenderingAttachmentInfo depthAttInfo {
    .imageView = depth_attachment.view,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .loadOp = vk::AttachmentLoadOp::eClear,
    .storeOp = vk::AttachmentStoreOp::eStore,
    .clearValue = vk::ClearDepthStencilValue{1.0f, 0}
  };
  vk::RenderingInfo renderInfo {
    .renderArea = scissor,
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size()),
    .pColorAttachments = attachmentInfos.size() ? attachmentInfos.data() : nullptr,
    .pDepthAttachment = depth_attachment.view ? &depthAttInfo : nullptr
  };
  VkRenderingInfo rInf = (VkRenderingInfo)renderInfo;
  vkCmdBeginRendering(commandBuffer, &rInf);
}

RenderTargetState::~RenderTargetState()
{
  vkCmdEndRendering(commandBuffer);
}
}
