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
  PipelineBase(PipelineManager* owner, PipelineId in_id, ShaderProgramId in_shader_program_id);
  PipelineBase() = default;
  ~PipelineBase();

private:
  PipelineManager* owner{nullptr};
  PipelineId id{PipelineId::Invalid};
  ShaderProgramId shaderProgramId{ShaderProgramId::Invalid};
};

} // namespace etna

#endif // ETNA_PIPELINE_BASE_HPP_INCLUDED
