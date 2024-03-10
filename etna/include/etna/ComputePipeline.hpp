#pragma once
#ifndef ETNA_COMPUTE_PIPELINE_HPP_INCLUDED
#define ETNA_COMPUTE_PIPELINE_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/VertexInput.hpp>
#include <etna/PipelineBase.hpp>


namespace etna
{

class PipelineManager;

class ComputePipeline : public PipelineBase
{
  friend class PipelineManager;
  ComputePipeline(PipelineManager* in_owner, PipelineId in_id, ShaderProgramId in_shader_program_id)
    : PipelineBase(in_owner, in_id, in_shader_program_id)
  {
  }

public:
  // Use PipelineManager to create pipelines
  ComputePipeline() = default;
  struct CreateInfo
  {
  };
};

} // namespace etna

#endif // ETNA_COMPUTE_PIPELINE_HPP_INCLUDED
