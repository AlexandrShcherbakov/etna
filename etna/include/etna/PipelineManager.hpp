#pragma once
#ifndef ETNA_PIPELINE_MANAGER_HPP_INCLUDED
#define ETNA_PIPELINE_MANAGER_HPP_INCLUDED

#include <unordered_map>

#include <etna/Vulkan.hpp>
#include <etna/PipelineBase.hpp>
#include <etna/GraphicsPipeline.hpp>


namespace etna
{

struct ShaderProgramManager;

class PipelineManager
{
  friend class PipelineBase;
public:
  PipelineManager(vk::Device dev, ShaderProgramManager& shader_manager);

  GraphicsPipeline createGraphicsPipeline(std::string shaderProgramName, GraphicsPipeline::CreateInfo info);
  // TODO: createComputePipeline, createRaytracePipeline, createMeshletPipeline

  void recreate();

private:
  void destroyPipeline(PipelineId id);
  vk::Pipeline getVkPipeline(PipelineId id) const;

private:
  vk::Device device;
  ShaderProgramManager& shaderManager;


  PipelineId pipelineIdCounter{0};
  struct PipelineParameters
  {
    std::string shaderProgramName;
    GraphicsPipeline::CreateInfo info;
  };
  std::unordered_map<PipelineId, vk::UniquePipeline> pipelines;
  std::unordered_multimap<PipelineId, PipelineParameters> graphicsPipelineParameters;
};

}

#endif // ETNA_PIPELINE_MANAGER_HPP_INCLUDED
