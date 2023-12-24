#include "etna/Quad.hpp"
#include "etna/GlobalContext.hpp"
#include "etna/Etna.hpp"
#include "etna/RenderTargetStates.hpp"
#include "etna/DescriptorSet.hpp"


namespace etna
{

void QuadRenderer::Create(const char *vspath, const char *fspath, CreateInfo info)
{
  m_programId = etna::create_program("quad_renderer", {fspath, vspath});

  auto &pipelineManager = etna::get_context().getPipelineManager();
  m_pipeline = pipelineManager.createGraphicsPipeline("quad_renderer",
    {
      .fragmentShaderOutput = 
      {
        .colorAttachmentFormats = {info.format}
      }
    });
}

void QuadRenderer::DrawCmd(VkCommandBuffer cmdBuff, VkImage targetImage, VkImageView targetImageView,
                           const Image &inTex, const Sampler &sampler)
{
  auto programInfo = get_shader_program(m_programId);
  auto set = create_descriptor_set(programInfo.getDescriptorLayoutId(0), cmdBuff,
    {
      Binding {0, inTex.genBinding(sampler.get(), vk::ImageLayout::eShaderReadOnlyOptimal)}
    });
  VkDescriptorSet vkSet = set.getVkSet();

  RenderTargetState::AttachmentParams colorAttachment{targetImage, targetImageView};
  colorAttachment.loadOp = vk::AttachmentLoadOp::eLoad;
  RenderTargetState renderTargets(cmdBuff, m_rect, {colorAttachment}, {});

  vkCmdBindPipeline(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline.getVkPipeline());
  vkCmdBindDescriptorSets(cmdBuff, VK_PIPELINE_BIND_POINT_GRAPHICS,
    m_pipeline.getVkPipelineLayout(), 0, 1, &vkSet, 0, VK_NULL_HANDLE);

  vkCmdDraw(cmdBuff, 3, 1, 0, 0);
}

}
