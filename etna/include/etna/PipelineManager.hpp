#pragma once
#ifndef ETNA_PIPELINE_MANAGER_HPP_INCLUDED
#define ETNA_PIPELINE_MANAGER_HPP_INCLUDED

#include <unordered_map>

#include <etna/Vulkan.hpp>
#include <etna/PipelineBase.hpp>
#include <etna/GraphicsPipeline.hpp>
#include <etna/ComputePipeline.hpp>


namespace etna
{

struct ShaderProgramManager;

class PipelineManager
{
  friend class PipelineBase;

public:
  PipelineManager(vk::Device dev, ShaderProgramManager& shader_manager);

  GraphicsPipeline createGraphicsPipeline(
    std::string shader_program_name, GraphicsPipeline::CreateInfo info);

  ComputePipeline createComputePipeline(
    std::string shader_program_name, ComputePipeline::CreateInfo info);
  // TODO: createRaytracePipeline, createMeshletPipeline

  void recreate();

private:
  void destroyPipeline(PipelineId id);
  vk::Pipeline getVkPipeline(PipelineId id) const;
  vk::PipelineLayout getVkPipelineLayout(ShaderProgramId id) const;

private:
  vk::Device device;
  ShaderProgramManager& shaderManager;


  PipelineId pipelineIdCounter{0};
  struct PipelineParameters
  {
    ShaderProgramId shaderProgram;
    GraphicsPipeline::CreateInfo info;
  };

  struct ComputeParameters
  {
    ShaderProgramId shaderProgram;
    ComputePipeline::CreateInfo info;
  };
  std::unordered_map<PipelineId, vk::UniquePipeline> pipelines;
  std::unordered_multimap<PipelineId, ComputeParameters> computePipelineParameters;
  std::unordered_multimap<PipelineId, PipelineParameters> graphicsPipelineParameters;
};

} // namespace etna

#endif // ETNA_PIPELINE_MANAGER_HPP_INCLUDED
