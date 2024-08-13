#pragma once
#ifndef ETNA_FORWARD_HPP_INCLUDED
#define ETNA_FORWARD_HPP_INCLUDED

#include <cstdint>


namespace etna
{

class PipelineManager;
enum class PipelineId : std::uint32_t
{
  Invalid = ~std::uint32_t{0}
};

struct ShaderProgramManager;
enum class ShaderProgramId : std::uint32_t
{
  Invalid = ~std::uint32_t{0}
};

} // namespace etna


#endif // ETNA_FORWARD_HPP_INCLUDED
