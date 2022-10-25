#pragma once
#ifndef ETNA_COMPUTE_PIPELINE_HPP_INCLUDED
#define ETNA_COMPUTE_PIPELINE_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/VertexInput.hpp>
#include <etna/PipelineBase.hpp>
#include <variant>

namespace etna
{

class PipelineManager;

class ComputePipeline: public PipelineBase
{
  friend class PipelineManager;
  ComputePipeline(PipelineManager *inOwner, PipelineId inId, ShaderProgramId inShaderProgramId)
    : PipelineBase(inOwner, inId, inShaderProgramId)
  {
  }
public:
  // Use PipelineManager to create pipelines
  ComputePipeline() = default;
  struct CreateInfo
  {
    // The only info we need is VkPipelineShaderStageCreateInfo
    vk::PipelineShaderStageCreateInfo stage = {};
  };

};
}

#endif // ETNA_COMPUTE_PIPELINE_HPP_INCLUDED
