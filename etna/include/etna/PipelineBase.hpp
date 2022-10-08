#pragma once
#ifndef ETNA_PIPELINE_BASE_HPP_INCLUDED
#define ETNA_PIPELINE_BASE_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{

class PipelineManager;

using PipelineId = std::size_t;
inline constexpr PipelineId INVALID_PIPELINE_ID = -1;

class PipelineBase
{
public:
  vk::Pipeline getVkPipeline() const;

  PipelineBase(const PipelineBase&) = delete;
  PipelineBase& operator=(const PipelineBase&) = delete;

  PipelineBase(PipelineBase&&) noexcept;
  PipelineBase& operator=(PipelineBase&&) noexcept;

protected:
  PipelineBase(PipelineManager* owner, PipelineId inId);
  PipelineBase() = default;
  ~PipelineBase();

private:
  PipelineManager* owner{nullptr};
  PipelineId id{INVALID_PIPELINE_ID};
};

}

#endif // ETNA_PIPELINE_BASE_HPP_INCLUDED
