#pragma once
#ifndef ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
#define ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED

#include <cstdint>
#include <unordered_map>
#include <string>
#include <variant>
#include <vector>
#include <span>


namespace vk
{
struct SpecializationInfo;
struct SpecializationMapEntry;
struct PipelineShaderStageCreateInfo;
} // namespace vk

namespace etna
{

using SpecializationConstant = std::pair<const char*, std::variant<bool, int32_t, float>>;
using SpecializationConstants = std::vector<SpecializationConstant>;
using SpecializationConstantsView = std::span<const SpecializationConstant>;

struct ShaderModuleSpecializationConstant
{
  enum class Type : uint8_t
  {
    Bool,
    // spv doesn't distinguish between signed and unsigned integers, so we don't either
    Int,
    Float,
  };

  uint32_t id;
  Type type;
};

using ShaderModuleSpecializationConstants =
  std::unordered_map<std::string, ShaderModuleSpecializationConstant>;

class ShaderProgramSpecConstsOverrider
{
public:
  explicit ShaderProgramSpecConstsOverrider(size_t num_stages, size_t const_storage_capacity);

  ShaderProgramSpecConstsOverrider(const ShaderProgramSpecConstsOverrider&) = delete;
  ShaderProgramSpecConstsOverrider& operator=(const ShaderProgramSpecConstsOverrider&) = delete;
  ShaderProgramSpecConstsOverrider(ShaderProgramSpecConstsOverrider&&) = default;
  ShaderProgramSpecConstsOverrider& operator=(ShaderProgramSpecConstsOverrider&&) = default;

  void overrideSpecializationConstants(
    vk::PipelineShaderStageCreateInfo& shader_info,
    const ShaderModuleSpecializationConstants& available_constants,
    SpecializationConstantsView overrides);

  std::string_view getLog() const { return log; }

private:
  std::string log;
  std::vector<vk::SpecializationInfo> specInfos;
  // Each constant's size is 4 bytes (64-bit constants are not currently supported)
  std::vector<uint32_t> specConstStorage;
  std::vector<vk::SpecializationMapEntry> specMapEntries;
};

} // namespace etna

#endif // ETNA_SPECIALIZATION_CONSTANTS_HPP_INCLUDED
