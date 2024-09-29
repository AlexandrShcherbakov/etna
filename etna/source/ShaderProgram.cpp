#include <etna/ShaderProgram.hpp>

#include <fstream>
#include <spirv_reflect.h>
#include <fmt/std.h>

#include <etna/GlobalContext.hpp>


namespace etna
{

ShaderModule::ShaderModule(vk::Device device, std::filesystem::path shader_path)
  : path{shader_path}
{
  reload(device);
}

static std::vector<char> read_file(std::filesystem::path filename)
{
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open())
  {
    ETNA_PANIC("Failed to open file {}", filename);
  }

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);
  file.close();
  return buffer;
}

struct SpvModDeleter
{
  SpvModDeleter() {}
  void operator()(SpvReflectShaderModule* mod) const
  {
    spvReflectDestroyShaderModule(mod);
    delete mod;
  }
};


#define ETNA_SPV_REFLECT_VERIFY(res, path)                                                         \
  ETNA_VERIFYF((res) == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V parse error in {}", (path))

void ShaderModule::reload(vk::Device device)
{
  vkModule = {};

  auto code = read_file(path);
  vk::ShaderModuleCreateInfo info{};
  info.setPCode(reinterpret_cast<const uint32_t*>(code.data()));
  info.setCodeSize(code.size());

  if (code.size() % 4 != 0)
    ETNA_PANIC("SPIRV {} broken", path);

  vkModule = unwrap_vk_result(device.createShaderModuleUnique(info));

  std::unique_ptr<SpvReflectShaderModule, SpvModDeleter> spvModule;
  spvModule.reset(new SpvReflectShaderModule{});

  ETNA_SPV_REFLECT_VERIFY(
    spvReflectCreateShaderModule(code.size(), code.data(), spvModule.get()), path);

  stage = static_cast<vk::ShaderStageFlagBits>(spvModule->shader_stage);
  entryPoint = spvModule->entry_point_name;

  resources.clear();

  uint32_t count = 0;
  ETNA_SPV_REFLECT_VERIFY(
    spvReflectEnumerateDescriptorSets(spvModule.get(), &count, nullptr), path);

  std::vector<SpvReflectDescriptorSet*> sets(count);
  ETNA_SPV_REFLECT_VERIFY(
    spvReflectEnumerateDescriptorSets(spvModule.get(), &count, sets.data()), path);

  resources.reserve(sets.size());

  for (auto pSet : sets)
  {
    DescriptorSetInfo dsInfo;
    dsInfo.clear();
    dsInfo.parseShader(stage, *pSet);
    resources.push_back({pSet->set, dsInfo});
  }

  if (spvModule->push_constant_block_count == 1)
  {
    auto& blk = spvModule->push_constant_blocks[0];
    if (blk.offset != 0)
    {
      ETNA_PANIC("SPIRV {} parse error: PushConst offset is not zero", path);
    }
    pushConst.stageFlags = stage;
    pushConst.offset = 0u;
    pushConst.size = blk.size;
  }
  else if (spvModule->push_constant_block_count > 1)
  {
    ETNA_PANIC("SPIRV {} parse error: only 1 push_const block per shader supported", path);
  }
  else
  {
    pushConst.size = 0u;
    pushConst.offset = 0u;
    pushConst.stageFlags = vk::ShaderStageFlags{};
  }
}

uint32_t ShaderProgramManager::registerModule(std::filesystem::path path)
{
  auto it = shaderModuleNames.find(path);
  if (it != shaderModuleNames.end())
    return it->second;

  uint32_t modId = static_cast<uint32_t>(shaderModules.size());
  std::unique_ptr<ShaderModule> newMod;
  newMod.reset(new ShaderModule{get_context().getDevice(), path});
  shaderModules.push_back(std::move(newMod));
  return modId;
}

static void validate_program_shaders(
  const std::string& name, const std::vector<vk::ShaderStageFlagBits>& stages)
{
  auto supportedShaders = vk::ShaderStageFlagBits::eVertex |
    vk::ShaderStageFlagBits::eTessellationControl |
    vk::ShaderStageFlagBits::eTessellationEvaluation | vk::ShaderStageFlagBits::eGeometry |
    vk::ShaderStageFlagBits::eFragment | vk::ShaderStageFlagBits::eCompute;

  bool isComputePipeline = false;
  vk::ShaderStageFlags usageMask = static_cast<vk::ShaderStageFlags>(0);

  for (auto stage : stages)
  {
    if (!(stage & supportedShaders))
    {
      ETNA_PANIC(
        "Shader program {} creating error, unsupported shader stage {}",
        name,
        vk::to_string(stage));
    }

    if (stage & usageMask)
    {
      ETNA_PANIC(
        "Shader program {} creating error, multiple usage of {} shader stage",
        name,
        vk::to_string(stage));
    }

    isComputePipeline |= (stage == vk::ShaderStageFlagBits::eCompute);
    usageMask |= stage;
  }

  if (isComputePipeline && stages.size() != 1)
  {
    ETNA_PANIC("Shader program {} creating error, usage of compute shader with other stages", name);
  }
}

ShaderProgramId ShaderProgramManager::loadProgram(
  const char* name, std::span<std::filesystem::path const> shaders_path)
{
  if (programNames.find(name) != programNames.end())
    ETNA_PANIC("Shader program {} redefenition", name);

  std::vector<uint32_t> moduleIds;
  std::vector<vk::ShaderStageFlagBits> stages;
  for (const auto& path : shaders_path)
  {
    auto id = registerModule(path);
    auto stage = getModule(id).getStage();

    moduleIds.push_back(id);
    stages.push_back(stage);
  }

  validate_program_shaders(name, stages);

  ShaderProgramId progId = static_cast<ShaderProgramId>(programs.size());
  programs.emplace_back(new ShaderProgramInternal{name, std::move(moduleIds)});
  programs[static_cast<std::underlying_type_t<ShaderProgramId>>(progId)]->reload(*this);
  programNames[name] = progId;
  return progId;
}

ShaderProgramId ShaderProgramManager::tryGetProgram(const char* name) const
{
  auto it = programNames.find(name);
  if (it == programNames.end())
    return ShaderProgramId::Invalid;
  return it->second;
}

ShaderProgramId ShaderProgramManager::getProgram(const char* name) const
{
  auto it = programNames.find(name);
  if (it == programNames.end())
    ETNA_PANIC("Shader program {} not found", name);
  return it->second;
}

void ShaderProgramManager::ShaderProgramInternal::reload(ShaderProgramManager& manager)
{
  progLayout = {};
  usedDescriptors = {};
  pushConst = vk::PushConstantRange{};

  std::array<DescriptorSetInfo, MAX_PROGRAM_DESCRIPTORS> dstDescriptors;
  auto& descriptorLayoutCache = get_context().getDescriptorSetLayouts();

  for (auto id : moduleIds)
  {
    auto& shaderMod = manager.getModule(id);

    if (shaderMod.getPushConst().size > 0) // merge push constants
    {
      auto modPushConst = shaderMod.getPushConst();
      if (pushConst.size == 0u)
      {
        pushConst = modPushConst;
      }
      else
      {
        ETNA_ASSERTF(
          pushConst.size == modPushConst.size,
          "ShaderProgram {}: push constant blocks differ between modules (stages), "
          "expected {} bytes but got {} bytes in module {}",
          name,
          pushConst.size,
          modPushConst.size,
          shaderMod.getName());
        pushConst.stageFlags |= modPushConst.stageFlags;
      }
    }

    const auto& resources = shaderMod.getResources(); // merge descriptors
    for (auto& desc : resources)
    {
      if (desc.first >= MAX_PROGRAM_DESCRIPTORS)
        ETNA_PANIC(
          "ShaderProgram {} : set {} out of max sets ({})",
          name,
          desc.first,
          MAX_PROGRAM_DESCRIPTORS);

      usedDescriptors.set(desc.first);
      dstDescriptors[desc.first].merge(desc.second);
    }
  }

  std::vector<vk::DescriptorSetLayout> vkLayouts;

  for (uint32_t i = 0; i < MAX_PROGRAM_DESCRIPTORS; i++)
  {
    if (!usedDescriptors.test(i))
      continue;
    auto res = descriptorLayoutCache.get(get_context().getDevice(), dstDescriptors[i]);
    descriptorIds[i] = res.first;
    vkLayouts.push_back(res.second);
  }

  vk::PipelineLayoutCreateInfo info{};
  info.setSetLayouts(vkLayouts);

  if (pushConst.size > 0)
  {
    info.setPPushConstantRanges(&pushConst);
    info.setPushConstantRangeCount(1u);
  }

  progLayout = unwrap_vk_result(get_context().getDevice().createPipelineLayoutUnique(info));
}

void ShaderProgramManager::reloadPrograms()
{
  for (auto& mod : shaderModules)
  {
    mod->reload(get_context().getDevice());
  }

  for (auto& progPtr : programs)
  {
    progPtr->reload(*this);
  }
}

void ShaderProgramManager::clear()
{
  programNames.clear();
  programs.clear();
  shaderModuleNames.clear();
  shaderModules.clear();
}

std::vector<vk::PipelineShaderStageCreateInfo> ShaderProgramManager::getShaderStages(
  ShaderProgramId id) const
{
  auto& prog = getProgInternal(id);

  std::vector<vk::PipelineShaderStageCreateInfo> stages;
  stages.reserve(prog.moduleIds.size());

  for (auto modId : prog.moduleIds)
  {
    const auto& shaderMod = getModule(modId);
    vk::PipelineShaderStageCreateInfo info{};
    info.setModule(shaderMod.getVkModule());
    info.setStage(shaderMod.getStage());
    info.setPName(shaderMod.getName().c_str());
    stages.push_back(info);
  }
  return stages;
}

vk::DescriptorSetLayout ShaderProgramManager::getDescriptorLayout(
  ShaderProgramId id, uint32_t set) const
{
  auto layoutId = getDescriptorLayoutId(id, set);
  return get_context().getDescriptorSetLayouts().getVkLayout(layoutId);
}

vk::PushConstantRange ShaderProgramInfo::getPushConst() const
{
  auto& prog = mgr.getProgInternal(id);
  return prog.pushConst;
}

vk::PipelineLayout ShaderProgramInfo::getPipelineLayout() const
{
  auto& prog = mgr.getProgInternal(id);
  return prog.progLayout.get();
}

bool ShaderProgramInfo::isDescriptorSetUsed(uint32_t set) const
{
  auto& prog = mgr.getProgInternal(id);
  return set < MAX_PROGRAM_DESCRIPTORS && prog.usedDescriptors.test(set);
}

DescriptorLayoutId ShaderProgramInfo::getDescriptorLayoutId(uint32_t set) const
{
  ETNA_VERIFY(isDescriptorSetUsed(set));
  return mgr.getProgInternal(id).descriptorIds.at(set);
}

vk::DescriptorSetLayout ShaderProgramInfo::getDescriptorSetLayout(uint32_t set) const
{
  return get_context().getDescriptorSetLayouts().getVkLayout(getDescriptorLayoutId(set));
}

const DescriptorSetInfo& ShaderProgramInfo::getDescriptorSetInfo(uint32_t set) const
{
  return get_context().getDescriptorSetLayouts().getLayoutInfo(getDescriptorLayoutId(set));
}

} // namespace etna
