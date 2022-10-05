#include <etna/GraphicsPipeline.hpp>

namespace etna
{

GraphicsPipeline::GraphicsPipeline(
  vk::Device device,
  vk::PipelineLayout layout,
  std::span<const vk::PipelineShaderStageCreateInfo> stages, 
  CreateInfo info)
  : cachedCreateInfo{std::move(info)}
{
  recreate(device, layout, stages);
}

void GraphicsPipeline::recreate(
  vk::Device device,
  vk::PipelineLayout layout,
  std::span<const vk::PipelineShaderStageCreateInfo> stages)
{
  pipeline = {};

  std::vector<vk::VertexInputAttributeDescription> vertexAttribures;
  std::vector<vk::VertexInputBindingDescription> vertexBindings;

  for (uint32_t i = 0; i < cachedCreateInfo.vertexShaderInput.bindings.size(); i++)
  {
    const auto& bindingDesc = cachedCreateInfo.vertexShaderInput.bindings[i];
    if (!bindingDesc.has_value())
      continue;
    
    vertexBindings.emplace_back() =
      vk::VertexInputBindingDescription
      {
        .binding = i,
        .stride = bindingDesc->byteStreamDescription.stride,
        .inputRate = bindingDesc->inputRate
      };

    for (uint32_t j = 0; j < bindingDesc->attributeMapping.size(); ++j)
    {
      const auto& attr = bindingDesc->byteStreamDescription
        .attributes[bindingDesc->attributeMapping[j]];
      vertexAttribures.emplace_back() =
        vk::VertexInputAttributeDescription
        {
          .location = j,
          .binding = i,
          .format = attr.format,
          .offset = attr.offset
        };
    }
  }

  vk::PipelineVertexInputStateCreateInfo vertexInput {};
  vertexInput.setVertexAttributeDescriptions(vertexAttribures);
  vertexInput.setVertexBindingDescriptions(vertexBindings);


  vk::PipelineViewportStateCreateInfo viewportState {};

  vk::PipelineMultisampleStateCreateInfo multisampleState
    {
      .rasterizationSamples = vk::SampleCountFlagBits::e1,
      .sampleShadingEnable = false,
    };

  vk::PipelineColorBlendStateCreateInfo blendState
    {
      .logicOpEnable = cachedCreateInfo.blendingConfig.logicOpEnable,
      .logicOp = cachedCreateInfo.blendingConfig.logicOp,
    };
  blendState.setAttachments(cachedCreateInfo.blendingConfig.attachments);
  blendState.blendConstants = cachedCreateInfo.blendingConfig.blendConstants;

  std::vector<vk::DynamicState> dynamicStates = {
    vk::DynamicState::eViewport,
    vk::DynamicState::eScissor
  };
  vk::PipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.setDynamicStates(dynamicStates);
  
  vk::GraphicsPipelineCreateInfo pipelineInfo
    {
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &cachedCreateInfo.inputAssemblyConfig,
      .pViewportState = &viewportState,
      .pRasterizationState = &cachedCreateInfo.rasterizationConfig,
      .pMultisampleState = &multisampleState,
      .pDepthStencilState = &cachedCreateInfo.depthConfig,
      .pColorBlendState = &blendState,
      .pDynamicState = &dynamicState,
      .layout = layout
    };
  pipelineInfo.setStages(stages);

  pipeline = device.createGraphicsPipelineUnique(nullptr, pipelineInfo);
}

}
