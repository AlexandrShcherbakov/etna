#pragma once
#ifndef ETNA_FORWARD_HPP_INCLUDED
#define ETNA_FORWARD_HPP_INCLUDED

#include <cstdint>

namespace etna
{

class PipelineManager;
using PipelineId = std::uint32_t;
inline constexpr PipelineId INVALID_PIPELINE_ID = static_cast<PipelineId>(-1);

struct ShaderProgramManager;
using ShaderProgramId = std::uint32_t;
inline constexpr ShaderProgramId INVALID_SHADER_PROGRAM_ID = static_cast<PipelineId>(-1);

}


#endif // ETNA_FORWARD_HPP_INCLUDED
