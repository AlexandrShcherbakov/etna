#include <etna/RenderTargetStates.hpp>

#include <etna/GlobalContext.hpp>
#include "StateTracking.hpp"


namespace etna
{

bool RenderTargetState::inScope = false;

RenderTargetState::RenderTargetState(
  vk::CommandBuffer cmd_buff,
  vk::Rect2D rect,
  const std::vector<AttachmentParams>& color_attachments,
  AttachmentParams depth_attachment,
  AttachmentParams stencil_attachment,
  BarrierBehavior behavior)
{
  ETNA_VERIFYF(!inScope, "RenderTargetState scopes shouldn't overlap.");
  inScope = true;
  // TODO: add resource state tracking
  commandBuffer = cmd_buff;
  vk::Viewport viewport{
    .x = static_cast<float>(rect.offset.x),
    .y = static_cast<float>(rect.offset.y),
    .width = static_cast<float>(rect.extent.width),
    .height = static_cast<float>(rect.extent.height),
    .minDepth = 0.0f,
    .maxDepth = 1.0f,
  };

  commandBuffer.setViewport(0, {viewport});
  commandBuffer.setScissor(0, {rect});

  std::vector<vk::RenderingAttachmentInfo> attachmentInfos(color_attachments.size());
  for (uint32_t i = 0; i < color_attachments.size(); ++i)
  {
    attachmentInfos[i].imageView = color_attachments[i].view;
    attachmentInfos[i].imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
    attachmentInfos[i].loadOp = color_attachments[i].loadOp;
    attachmentInfos[i].storeOp = color_attachments[i].storeOp;
    attachmentInfos[i].clearValue = color_attachments[i].clearColorValue;

    etna::get_context().getResourceTracker().setColorTarget(
      commandBuffer, color_attachments[i].image, behavior);

    if (color_attachments[i].resolveImage)
    {
      etna::get_context().getResourceTracker().setResolveTarget(
        commandBuffer,
        color_attachments[i].resolveImage,
        vk::ImageAspectFlagBits::eColor,
        behavior);

      attachmentInfos[i].resolveImageLayout = vk::ImageLayout::eGeneral;
      attachmentInfos[i].resolveImageView = color_attachments[i].resolveImageView;
      attachmentInfos[i].resolveMode = color_attachments[i].resolveMode;
    }
  }

  vk::RenderingAttachmentInfo depthAttInfo{
    .imageView = depth_attachment.view,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = depth_attachment.resolveMode,
    .resolveImageView = depth_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = depth_attachment.loadOp,
    .storeOp = depth_attachment.storeOp,
    .clearValue = depth_attachment.clearDepthStencilValue,
  };

  vk::RenderingAttachmentInfo stencilAttInfo{
    .imageView = stencil_attachment.view,
    .imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal,
    .resolveMode = stencil_attachment.resolveMode,
    .resolveImageView = stencil_attachment.resolveImageView,
    .resolveImageLayout = vk::ImageLayout::eGeneral,
    .loadOp = stencil_attachment.loadOp,
    .storeOp = stencil_attachment.storeOp,
    .clearValue = stencil_attachment.clearDepthStencilValue,
  };

  if (depth_attachment.image && stencil_attachment.image)
  {
    ETNA_VERIFYF(
      depth_attachment.view == stencil_attachment.view,
      "depth and stencil attachments must be created from the same image");
    etna::get_context().getResourceTracker().setDepthStencilTarget(
      commandBuffer,
      depth_attachment.image,
      vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
      behavior);

    if (depth_attachment.resolveImage && stencil_attachment.resolveImage)
    {
      etna::get_context().getResourceTracker().setResolveTarget(
        commandBuffer,
        depth_attachment.resolveImage,
        vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil,
        behavior);
    }
  }
  else
  {
    if (depth_attachment.image)
    {
      etna::get_context().getResourceTracker().setDepthStencilTarget(
        commandBuffer,
        depth_attachment.image,
        depth_attachment.imageAspect.value_or(vk::ImageAspectFlagBits::eDepth),
        behavior);

      if (depth_attachment.resolveImage)
      {
        etna::get_context().getResourceTracker().setResolveTarget(
          commandBuffer,
          depth_attachment.resolveImage,
          depth_attachment.resolveImageAspect.value_or(vk::ImageAspectFlagBits::eDepth),
          behavior);
      }
    }

    if (stencil_attachment.image)
    {
      etna::get_context().getResourceTracker().setDepthStencilTarget(
        commandBuffer,
        stencil_attachment.image,
        stencil_attachment.imageAspect.value_or(vk::ImageAspectFlagBits::eStencil),
        behavior);

      if (stencil_attachment.resolveImage)
      {
        etna::get_context().getResourceTracker().setResolveTarget(
          commandBuffer,
          stencil_attachment.resolveImage,
          stencil_attachment.resolveImageAspect.value_or(vk::ImageAspectFlagBits::eStencil),
          behavior);
      }
    }
  }

  etna::get_context().getResourceTracker().flushBarriers(commandBuffer);

  vk::RenderingInfo renderInfo{
    .renderArea = rect,
    .layerCount = 1,
    .colorAttachmentCount = static_cast<uint32_t>(attachmentInfos.size()),
    .pColorAttachments = attachmentInfos.empty() ? nullptr : attachmentInfos.data(),
    .pDepthAttachment = depth_attachment.view ? &depthAttInfo : nullptr,
    .pStencilAttachment = stencil_attachment.view ? &stencilAttInfo : nullptr,
  };
  commandBuffer.beginRendering(renderInfo);
}

RenderTargetState::~RenderTargetState()
{
  commandBuffer.endRendering();
  inScope = false;
}

} // namespace etna
