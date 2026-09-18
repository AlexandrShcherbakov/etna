#include <etna/PipelineManager.hpp>

#include <span>
#include <vector>

#include <etna/Assert.hpp>
#include <etna/ShaderProgram.hpp>
#include <etna/VulkanFormatter.hpp>
#include <etna/SpecializationConstants.hpp>

namespace etna
{

static vk::UniquePipeline createComputePipelineInternal(
  vk::Device device, vk::PipelineLayout layout, const vk::PipelineShaderStageCreateInfo stage)
{
  vk::ComputePipelineCreateInfo pipelineInfo{.layout = layout};
  pipelineInfo.setStage(stage);

  return unwrap_vk_result(device.createComputePipelineUnique(nullptr, pipelineInfo));
}


static vk::UniquePipeline create_graphics_pipeline_internal(
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

    vertexBindings.emplace_back() = vk::VertexInputBindingDescription{
      .binding = i,
      .stride = bindingDesc->byteStreamDescription.stride,
      .inputRate = bindingDesc->inputRate,
    };

    for (uint32_t j = 0; j < bindingDesc->attributeMapping.size(); ++j)
    {
      const auto& attr =
        bindingDesc->byteStreamDescription.attributes[bindingDesc->attributeMapping[j]];
      vertexAttribures.emplace_back() = vk::VertexInputAttributeDescription{
        .location = j,
        .binding = i,
        .format = attr.format,
        .offset = attr.offset,
      };
    }
  }

  vk::PipelineVertexInputStateCreateInfo vertexInput{};
  vertexInput.setVertexAttributeDescriptions(vertexAttribures);
  vertexInput.setVertexBindingDescriptions(vertexBindings);


  vk::PipelineViewportStateCreateInfo viewportState{
    .viewportCount = 1,
    .scissorCount = 1,
  };

  vk::PipelineColorBlendStateCreateInfo blendState{
    .logicOpEnable = static_cast<vk::Bool32>(info.blendingConfig.logicOpEnable),
    .logicOp = info.blendingConfig.logicOp,
  };
  blendState.setAttachments(info.blendingConfig.attachments);
  blendState.blendConstants = info.blendingConfig.blendConstants;

  vk::PipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.setDynamicStates(info.dynamicStates);

  vk::PipelineRenderingCreateInfo rendering{
    .depthAttachmentFormat = info.fragmentShaderOutput.depthAttachmentFormat,
    .stencilAttachmentFormat = info.fragmentShaderOutput.stencilAttachmentFormat,
  };
  rendering.setColorAttachmentFormats(info.fragmentShaderOutput.colorAttachmentFormats);

  vk::GraphicsPipelineCreateInfo pipelineInfo{
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
    .layout = layout,
  };
  pipelineInfo.setStages(stages);

  return unwrap_vk_result(device.createGraphicsPipelineUnique(nullptr, pipelineInfo));
}

PipelineManager::PipelineManager(vk::Device dev, ShaderProgramManager& shader_manager)
  : device{dev}
  , shaderManager{shader_manager}
{
}
struct AllStagesSpecConstsView
{
  SpecializationConstantsView vertexSpecConsts{};
  SpecializationConstantsView tessellationControlSpecConsts{};
  SpecializationConstantsView tessellationEvalSpecConsts{};
  SpecializationConstantsView geometrySpecConsts{};
  SpecializationConstantsView fragmentSpecConsts{};
  SpecializationConstantsView computeSpecConsts{};
};

static std::pair<size_t, size_t> get_params_for_spec_consts_overrider(
  std::span<const vk::PipelineShaderStageCreateInfo> shader_stages,
  std::span<const ShaderModuleSpecializationConstants> shader_spec_consts,
  AllStagesSpecConstsView all_stages_overrides)
{
  size_t stagesWithSpecConsts = 0;
  size_t specConstsMaxCount = 0;
  for (size_t i = 0; i < shader_stages.size(); ++i)
  {
    const auto& stageInfo = shader_stages[i];
    const auto& specConsts = shader_spec_consts[i];
    if (specConsts.empty())
      continue;

    SpecializationConstantsView overrides{};
    switch (stageInfo.stage)
    {
    case vk::ShaderStageFlagBits::eVertex:
      overrides = all_stages_overrides.vertexSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eTessellationControl:
      overrides = all_stages_overrides.tessellationControlSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
      overrides = all_stages_overrides.tessellationEvalSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eGeometry:
      overrides = all_stages_overrides.geometrySpecConsts;
      break;
    case vk::ShaderStageFlagBits::eFragment:
      overrides = all_stages_overrides.fragmentSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eCompute:
      overrides = all_stages_overrides.computeSpecConsts;
      break;
    default:
      spdlog::warn(
        "Unsupported shader stage {} for specialization constants overrides, ignoring",
        vk::to_string(stageInfo.stage));
    }

    if (!overrides.empty())
    {
      stagesWithSpecConsts++;
      specConstsMaxCount += std::min(overrides.size(), specConsts.size());
    }
  }

  return std::make_pair(stagesWithSpecConsts, specConstsMaxCount);
}

static void override_spec_consts(
  ShaderProgramSpecConstsOverrider& overrider,
  std::span<vk::PipelineShaderStageCreateInfo> shader_stages,
  std::span<const ShaderModuleSpecializationConstants> shader_spec_consts,
  AllStagesSpecConstsView all_stages_overrides)
{
  for (size_t i = 0; i < shader_stages.size(); ++i)
  {
    auto& stageInfo = shader_stages[i];
    const auto& specConsts = shader_spec_consts[i];
    SpecializationConstantsView overrides{};
    switch (stageInfo.stage)
    {
    case vk::ShaderStageFlagBits::eVertex:
      overrides = all_stages_overrides.vertexSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eTessellationControl:
      overrides = all_stages_overrides.tessellationControlSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eTessellationEvaluation:
      overrides = all_stages_overrides.tessellationEvalSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eGeometry:
      overrides = all_stages_overrides.geometrySpecConsts;
      break;
    case vk::ShaderStageFlagBits::eFragment:
      overrides = all_stages_overrides.fragmentSpecConsts;
      break;
    case vk::ShaderStageFlagBits::eCompute:
      overrides = all_stages_overrides.computeSpecConsts;
      break;
    default:
      break;
    }
    overrider.overrideSpecializationConstants(stageInfo, specConsts, overrides);
  }
}

ComputePipeline PipelineManager::createComputePipeline(
  const char* shader_program_name, ComputePipeline::CreateInfo info)
{
  const PipelineId pipelineId = static_cast<PipelineId>(pipelineIdCounter++);
  const ShaderProgramId progId = shaderManager.getProgram(shader_program_name);

  auto [shaderStages, shaderSpecConsts] = shaderManager.getShaderStages(progId);

  ETNA_VERIFYF(
    shaderStages.size() == 1,
    "Incorrect shader program, expected 1 stage for ComputePipeline, but got {}!",
    shaderStages.size());

  auto [stagesWithSpecConsts, specConstsMaxCount] = get_params_for_spec_consts_overrider(
    shaderStages, shaderSpecConsts, {.computeSpecConsts = info.specializationConstants});

  ShaderProgramSpecConstsOverrider specConstsOverrider(stagesWithSpecConsts, specConstsMaxCount);
  override_spec_consts(
    specConstsOverrider,
    shaderStages,
    shaderSpecConsts,
    {.computeSpecConsts = info.specializationConstants});

  pipelines.emplace(
    pipelineId,
    createComputePipelineInternal(device, shaderManager.getProgramLayout(progId), shaderStages[0]));
  computePipelineParameters.emplace(pipelineId, ComputeParameters{progId, std::move(info)});

  if (auto log = specConstsOverrider.getLog(); !log.empty())
    spdlog::info("Program Info for '{}':\n{}", shader_program_name, log);

  return ComputePipeline(this, pipelineId, progId);
};

static void print_prog_info(
  const etna::ShaderProgramInfo& info, const std::string& name, std::string_view spec_consts_log)
{
  std::string result;
  auto it = std::back_inserter(result);

  for (uint32_t set = 0u; set < etna::MAX_PROGRAM_DESCRIPTORS; set++)
  {
    if (!info.isDescriptorSetUsed(set))
      continue;

    fmt::format_to(it, " Set {}:\n", set);
    const auto& setInfo = info.getDescriptorSetInfo(set);
    for (uint32_t binding = 0; binding < etna::MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (!setInfo.isBindingUsed(binding))
        continue;
      const auto& vkBinding = setInfo.getBinding(binding);

      fmt::format_to(
        it,
        "  Binding {}: {}, count = {}, stages = {}\n",
        binding,
        vkBinding.descriptorType,
        vkBinding.descriptorCount,
        vkBinding.stageFlags);
    }
  }

  auto pc = info.getPushConst();
  if (pc.size > 0)
    fmt::format_to(it, "  PushConst size = {}, stages = {}\n", pc.size, pc.stageFlags);

  spdlog::info("Program Info for '{}':\n{}\n{}", name, result, spec_consts_log);
}

GraphicsPipeline PipelineManager::createGraphicsPipeline(
  const char* shader_program_name, GraphicsPipeline::CreateInfo info)
{
  const PipelineId pipelineId = static_cast<PipelineId>(pipelineIdCounter++);
  const ShaderProgramId progId = shaderManager.getProgram(shader_program_name);

  auto [shaderStages, shaderSpecConsts] = shaderManager.getShaderStages(progId);

  AllStagesSpecConstsView allStagesSpecConstsOverrides{
    .vertexSpecConsts = info.vertexSpecConsts,
    .tessellationControlSpecConsts = info.tessellationControlSpecConsts,
    .tessellationEvalSpecConsts = info.tessellationEvalSpecConsts,
    .geometrySpecConsts = info.geometrySpecConsts,
    .fragmentSpecConsts = info.fragmentSpecConsts,
  };
  auto [stagesWithSpecConsts, specConstsMaxCount] = get_params_for_spec_consts_overrider(
    shaderStages, shaderSpecConsts, allStagesSpecConstsOverrides);

  ShaderProgramSpecConstsOverrider specConstsOverrider(stagesWithSpecConsts, specConstsMaxCount);
  override_spec_consts(
    specConstsOverrider, shaderStages, shaderSpecConsts, allStagesSpecConstsOverrides);

  pipelines.emplace(
    pipelineId,
    create_graphics_pipeline_internal(
      device, shaderManager.getProgramLayout(progId), shaderStages, info));
  graphicsPipelineParameters.emplace(pipelineId, PipelineParameters{progId, std::move(info)});

  GraphicsPipeline pipeline(this, pipelineId, progId);
  print_prog_info(
    shaderManager.getProgramInfo(shader_program_name),
    shader_program_name,
    specConstsOverrider.getLog());
  return pipeline;
}

void PipelineManager::recreate()
{
  pipelines.clear();
  for (const auto& [id, params] : graphicsPipelineParameters)
  {
    auto [shaderStages, shaderSpecConsts] = shaderManager.getShaderStages(params.shaderProgram);

    AllStagesSpecConstsView allStagesSpecConstsOverrides{
      .vertexSpecConsts = params.info.vertexSpecConsts,
      .tessellationControlSpecConsts = params.info.tessellationControlSpecConsts,
      .tessellationEvalSpecConsts = params.info.tessellationEvalSpecConsts,
      .geometrySpecConsts = params.info.geometrySpecConsts,
      .fragmentSpecConsts = params.info.fragmentSpecConsts,
    };
    auto [stagesWithSpecConsts, specConstsMaxCount] = get_params_for_spec_consts_overrider(
      shaderStages, shaderSpecConsts, allStagesSpecConstsOverrides);

    ShaderProgramSpecConstsOverrider specConstsOverrider(stagesWithSpecConsts, specConstsMaxCount);
    override_spec_consts(
      specConstsOverrider, shaderStages, shaderSpecConsts, allStagesSpecConstsOverrides);

    pipelines.emplace(
      id,
      create_graphics_pipeline_internal(
        device, shaderManager.getProgramLayout(params.shaderProgram), shaderStages, params.info));
  }
  for (const auto& [id, params] : computePipelineParameters)
  {
    auto [shaderStages, shaderSpecConsts] = shaderManager.getShaderStages(params.shaderProgram);

    auto [stagesWithSpecConsts, specConstsMaxCount] = get_params_for_spec_consts_overrider(
      shaderStages, shaderSpecConsts, {.computeSpecConsts = params.info.specializationConstants});

    ShaderProgramSpecConstsOverrider specConstsOverrider(stagesWithSpecConsts, specConstsMaxCount);
    override_spec_consts(
      specConstsOverrider,
      shaderStages,
      shaderSpecConsts,
      {.computeSpecConsts = params.info.specializationConstants});

    pipelines.emplace(
      id,
      createComputePipelineInternal(
        device, shaderManager.getProgramLayout(params.shaderProgram), shaderStages[0]));
  }
}

void PipelineManager::destroyPipeline(PipelineId id)
{
  if (id == PipelineId::Invalid)
    return;

  pipelines.erase(id);
  graphicsPipelineParameters.erase(id);
}

vk::Pipeline PipelineManager::getVkPipeline(PipelineId id) const
{
  ETNA_VERIFY(id != PipelineId::Invalid);
  return pipelines.find(id)->second.get();
}

vk::PipelineLayout PipelineManager::getVkPipelineLayout(ShaderProgramId id) const
{
  ETNA_VERIFY(id != ShaderProgramId::Invalid);
  return shaderManager.getProgramLayout(id);
}

} // namespace etna
