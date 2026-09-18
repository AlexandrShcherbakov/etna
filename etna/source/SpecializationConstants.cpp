#include <etna/SpecializationConstants.hpp>
#include <etna/Vulkan.hpp>


namespace etna
{

std::string to_string(ShaderModuleSpecializationConstant::Type type)
{
  switch (type)
  {
  case ShaderModuleSpecializationConstant::Type::Bool:
    return "bool";
  case ShaderModuleSpecializationConstant::Type::Int:
    return "int";
  case ShaderModuleSpecializationConstant::Type::Float:
    return "float";
  default:
    return "unknown";
  }
}

ShaderProgramSpecConstsOverrider::ShaderProgramSpecConstsOverrider(
  size_t num_stages, size_t const_storage_capacity)
  : specInfos()
  , specConstStorage()
  , specMapEntries()
{
  specInfos.reserve(num_stages);
  specMapEntries.reserve(const_storage_capacity);
  specConstStorage.reserve(const_storage_capacity);
}

void ShaderProgramSpecConstsOverrider::overrideSpecializationConstants(
  vk::PipelineShaderStageCreateInfo& shader_info,
  const ShaderModuleSpecializationConstants& available_constants,
  SpecializationConstantsView overrides)
{
  if (overrides.empty())
    return;

  std::string stageLog;
  auto logIt = std::back_inserter(stageLog);

  auto specMapEntriesStart = specMapEntries.end();
  auto specConstStorageStart = specConstStorage.end();
  for (const auto& [name, value] : overrides)
  {
    auto it = available_constants.find(name);
    if (it == available_constants.end())
    {
      fmt::format_to(logIt, "  [Warning] Specialization constant {} not found, ignoring\n", name);
      continue;
    }

    auto [overrideType, bits] = std::visit(
      [](auto&& arg) -> std::pair<ShaderModuleSpecializationConstant::Type, uint32_t> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>)
          return {ShaderModuleSpecializationConstant::Type::Bool, static_cast<uint32_t>(arg)};
        else if constexpr (std::is_same_v<T, int32_t>)
          return {ShaderModuleSpecializationConstant::Type::Int, static_cast<uint32_t>(arg)};
        else if constexpr (std::is_same_v<T, float>)
          return {ShaderModuleSpecializationConstant::Type::Float, std::bit_cast<uint32_t>(arg)};
        else
          // can't use static_assert due to support of old compilers
          ETNA_PANIC("Unsupported specialization constant type");
      },
      value);

    const auto& specConst = it->second;
    if (specConst.type != overrideType)
    {
      fmt::format_to(
        logIt,
        "  [Warning] Specialization constant {} type mismatch (expected: {}, got: {}), "
        "ignoring\n",
        name,
        to_string(specConst.type),
        to_string(overrideType));
      continue;
    }

    const uint32_t offset = static_cast<uint32_t>(specConstStorage.size() * sizeof(bits));
    specConstStorage.push_back(bits);
    specMapEntries.push_back(vk::SpecializationMapEntry{specConst.id, offset, sizeof(bits)});

    fmt::format_to(
      logIt,
      "  Overriding specialization constant {} (id: {}) with value {}\n",
      name,
      specConst.id,
      std::visit([](auto&& arg) -> std::string { return std::to_string(arg); }, value));
  }

  if (!stageLog.empty())
  {
    fmt::format_to(
      std::back_inserter(log),
      " Specialization constants for shader stage {}:\n{}",
      vk::to_string(shader_info.stage),
      stageLog);
  }

  if (specMapEntriesStart != specMapEntries.end())
  {
    // Someone should punish vkhpp dev. This is ridiculous.
    // Why do I need an lvalue span and ArrayProxyNoTemporaries<const ...>
    // Deduction guides were added to make this easier, but ofc let's ignore them.
    std::span<const uint32_t> specConstStorageView(specConstStorageStart, specConstStorage.end());
    std::span<const vk::SpecializationMapEntry> specMapEntriesView(
      specMapEntriesStart, specMapEntries.end());

    specInfos.push_back(
      vk::SpecializationInfo{}
        .setData(vk::ArrayProxyNoTemporaries<const uint32_t>(specConstStorageView))
        .setMapEntries(
          vk::ArrayProxyNoTemporaries<const vk::SpecializationMapEntry>(specMapEntriesView)));
    shader_info.setPSpecializationInfo(std::addressof(specInfos.back()));
  }
}
} // namespace etna
