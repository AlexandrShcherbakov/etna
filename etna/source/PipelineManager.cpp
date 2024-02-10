#include "etna/Assert.hpp"
#include <etna/PipelineManager.hpp>

#include <iostream>
#include <span>
#include <vector>
#include <etna/ShaderProgram.hpp>


namespace etna
{

static vk::UniquePipeline createComputePipelineInternal(
  vk::Device device,
  vk::PipelineLayout layout,
  const vk::PipelineShaderStageCreateInfo stage)
{
  
  vk::ComputePipelineCreateInfo pipelineInfo
    {
      .layout = layout
    };
  pipelineInfo.setStage(stage);

  return device.createComputePipelineUnique(nullptr, pipelineInfo).value;
}


vk::UniquePipeline createGraphicsPipelineInternal(
  vk::Device device,
  vk::PipelineLayout layout,
  std::span<const vk::PipelineShaderStageCreateInfo> stages,
  const GraphicsPipeline::CreateInfo& info)
{
  std::vector<vk::VertexInputAttributeDescription> vertexAttribures;
  std::vector<vk::VertexInputBindingDescription> vertexBindings;

  for (uint32_t i = 0; i < info.vertexShaderInput.bindings.size(); i++)
  {
    const auto& bindingDesc = info.vertexShaderInput.bindings[i];
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


  vk::PipelineViewportStateCreateInfo viewportState
  {
    .viewportCount = 1,
    .scissorCount = 1,
  };

  vk::PipelineColorBlendStateCreateInfo blendState
    {
      .logicOpEnable = info.blendingConfig.logicOpEnable,
      .logicOp = info.blendingConfig.logicOp,
    };
  blendState.setAttachments(info.blendingConfig.attachments);
  blendState.blendConstants = info.blendingConfig.blendConstants;

  vk::PipelineDynamicStateCreateInfo dynamicState {};
  dynamicState.setDynamicStates(info.dynamicStates);

  vk::PipelineRenderingCreateInfo rendering 
    {
      .depthAttachmentFormat = info.fragmentShaderOutput.depthAttachmentFormat,
      .stencilAttachmentFormat = info.fragmentShaderOutput.stencilAttachmentFormat
    };
  rendering.setColorAttachmentFormats(info.fragmentShaderOutput.colorAttachmentFormats);
  
  vk::GraphicsPipelineCreateInfo pipelineInfo
    {
      .pNext = &rendering,
      .pVertexInputState = &vertexInput,
      .pInputAssemblyState = &info.inputAssemblyConfig,
      .pTessellationState = &info.tessellationConfig,
      .pViewportState = &viewportState,
      .pRasterizationState = &info.rasterizationConfig,
      .pMultisampleState = &info.multisampleConfig,
      .pDepthStencilState = &info.depthConfig,
      .pColorBlendState = &blendState,
      .pDynamicState = &dynamicState,
      .layout = layout
    };
  pipelineInfo.setStages(stages);

  return device.createGraphicsPipelineUnique(nullptr, pipelineInfo).value;
}

PipelineManager::PipelineManager(vk::Device dev, ShaderProgramManager& shader_manager)
  : device{dev}
  , shaderManager{shader_manager}
{

}

ComputePipeline PipelineManager::createComputePipeline(std::string shader_program_name, ComputePipeline::CreateInfo info)
{
  const PipelineId pipelineId = pipelineIdCounter++;  
  const ShaderProgramId progId = shaderManager.getProgram(shader_program_name);
  const std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = shaderManager.getShaderStages(progId);

  ETNA_ASSERTF(shaderStages.size() == 1,
    "Incorrect shader program, expected 1 stage for ComputePipeline, but got {}!",
    shaderStages.size());

  pipelines.emplace(pipelineId,
    createComputePipelineInternal(device,
      shaderManager.getProgramLayout(progId),
      shaderStages[0]));
  computePipelineParameters.emplace(pipelineId, ComputeParameters{progId, std::move(info)});
  
  return ComputePipeline(this, pipelineId, progId);
};

static void print_prog_info(const etna::ShaderProgramInfo &info, const std::string &name)
{
  std::cout << "Program Info " << name << "\n";

  for (uint32_t set = 0u; set < etna::MAX_PROGRAM_DESCRIPTORS; set++)
  {
    if (!info.isDescriptorSetUsed(set))
      continue;
    auto setInfo = info.getDescriptorSetInfo(set);
    for (uint32_t binding = 0; binding < etna::MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (!setInfo.isBindingUsed(binding))
        continue;
      auto &vkBinding = setInfo.getBinding(binding);

      std::cout << "Binding " << binding << " " << vk::to_string(vkBinding.descriptorType) << ", count = " << vkBinding.descriptorCount << " ";
      std::cout << " " << vk::to_string(vkBinding.stageFlags) << "\n"; 
    }
  }

  auto pc = info.getPushConst();
  if (pc.size)
  {
    std::cout << "PushConst " << " size = " << pc.size << " stages = " << vk::to_string(pc.stageFlags) << "\n";
  }
}

GraphicsPipeline PipelineManager::createGraphicsPipeline(std::string shader_program_name, GraphicsPipeline::CreateInfo info)
{
  const PipelineId pipelineId = pipelineIdCounter++;  
  const ShaderProgramId progId = shaderManager.getProgram(shader_program_name);

  pipelines.emplace(pipelineId,
    createGraphicsPipelineInternal(device,
      shaderManager.getProgramLayout(progId),
      shaderManager.getShaderStages(progId), info));
  graphicsPipelineParameters.emplace(pipelineId, PipelineParameters{progId, std::move(info)});
  
  GraphicsPipeline pipeline(this, pipelineId, progId);
  print_prog_info(shaderManager.getProgramInfo(shader_program_name), shader_program_name);
  return pipeline;
}

void PipelineManager::recreate()
{
  pipelines.clear();
  for (const auto&[id, params] : graphicsPipelineParameters)
    pipelines.emplace(id, 
      createGraphicsPipelineInternal(device,
        shaderManager.getProgramLayout(params.shaderProgram),
        shaderManager.getShaderStages(params.shaderProgram), params.info));
  for (const auto&[id, params] : computePipelineParameters)
    pipelines.emplace(id,
      createComputePipelineInternal(device,
        shaderManager.getProgramLayout(params.shaderProgram),
        shaderManager.getShaderStages(params.shaderProgram)[0]));
}

void PipelineManager::destroyPipeline(PipelineId id)
{
  if (id == INVALID_PIPELINE_ID)
    return;
  
  pipelines.erase(id);
  graphicsPipelineParameters.erase(id);
}

vk::Pipeline PipelineManager::getVkPipeline(PipelineId id) const
{
  ETNA_ASSERT(id != INVALID_PIPELINE_ID);
  return pipelines.find(id)->second.get();
}

vk::PipelineLayout PipelineManager::getVkPipelineLayout(ShaderProgramId id) const
{
  ETNA_ASSERT(id != INVALID_SHADER_PROGRAM_ID);
  return shaderManager.getProgramLayout(id);
}

}
