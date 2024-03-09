#pragma once
#ifndef ETNA_PIPELINE_BASE_HPP_INCLUDED
#define ETNA_PIPELINE_BASE_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/Forward.hpp>


namespace etna
{

class PipelineBase
{
public:
  vk::PipelineLayout getVkPipelineLayout() const;
  vk::Pipeline getVkPipeline() const;

  PipelineBase(const PipelineBase&) = delete;
  PipelineBase& operator=(const PipelineBase&) = delete;

  PipelineBase(PipelineBase&&) noexcept;
  PipelineBase& operator=(PipelineBase&&) noexcept;

protected:
  PipelineBase(PipelineManager* owner, PipelineId inId, ShaderProgramId inShaderProgramId);
  PipelineBase() = default;
  ~PipelineBase();

private:
  PipelineManager* owner{nullptr};
  PipelineId id{INVALID_PIPELINE_ID};
  ShaderProgramId shaderProgramId{INVALID_SHADER_PROGRAM_ID};
};

} // namespace etna

#endif // ETNA_PIPELINE_BASE_HPP_INCLUDED
