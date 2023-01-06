#include <etna/ShaderProgram.hpp>
#include <etna/DescriptorSetLayout.hpp>

#include <fstream>
#include <fmt/std.h>

#include <SpvReflectHelpers.hpp>


namespace etna
{
  ShaderModule::ShaderModule(vk::Device device, std::filesystem::path shader_path)
    : path {std::move(shader_path)}
  {
    reload(device);
  }

  static std::vector<std::byte> read_file(std::filesystem::path filename)
  {
    std::basic_ifstream<std::byte> file(filename, std::ios::ate | std::ios::binary);

    ETNA_ASSERTF(file.is_open(), "Failed to open file '{}'", filename);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<std::byte> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    ETNA_ASSERTF(file.gcount() == buffer.size(), "Failed to read file '{}'", filename);

    return buffer;
  }

  void ShaderModule::reload(vk::Device device)
  {
    vkModule = {};

    auto code = read_file(path);
    vk::ShaderModuleCreateInfo info {};
    info.setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    info.setCodeSize(code.size());

    ETNA_ASSERTF(code.size() % 4 == 0, "SPIR-V file '{}' is corrupted!", path);

    vkModule = device.createShaderModuleUnique(info).value;

    std::unique_ptr<SpvReflectShaderModule, void(*)(SpvReflectShaderModule*)> spvModule
      {new SpvReflectShaderModule, spvReflectDestroyShaderModule};


    SPV_REFLECT_SAFE_CALL(spvReflectCreateShaderModule(code.size(), code.data(), spvModule.get()), path);

    stage = static_cast<vk::ShaderStageFlagBits>(spvModule->shader_stage);
    entryPoint = spvModule->entry_point_name;

    resources.clear();

    uint32_t count = 0;
    SPV_REFLECT_SAFE_CALL(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, nullptr), path);

    std::vector<SpvReflectDescriptorSet*> sets(count);
    SPV_REFLECT_SAFE_CALL(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, sets.data()), path);

    resources.reserve(sets.size());

    for (auto pSet : sets)
    {
      DescriptorSetInfo info;
      info.clear();
      info.parseShader(stage, *pSet);
      resources.push_back({pSet->set, info});
    }

    if (spvModule->push_constant_block_count == 1)
    {
      auto &blk = spvModule->push_constant_blocks[0];

      ETNA_ASSERTF(blk.offset == 0, "SPIR-V file '{}' parse error: PushConst offset is not zero", path);

      pushConst.stageFlags = stage;
      pushConst.offset = 0u;
      pushConst.size = blk.size;
    }
    else if (spvModule->push_constant_block_count > 1)
    {
      ETNA_PANIC("SPIR-V file '{}' parse error: only 1 push_const block per shader supported", path);
    }
    else
    {
      pushConst.size = 0u;
      pushConst.offset = 0u;
      pushConst.stageFlags = static_cast<vk::ShaderStageFlags>(0u);
    }
  }

  uint32_t ShaderProgramManager::registerModule(const std::filesystem::path& path)
  {
    auto it = shaderModulePathToId.find(path);
    if (it != shaderModulePathToId.end())
      return it->second;

    uint32_t modId = static_cast<uint32_t>(shaderModules.size());

    shaderModules.push_back(std::make_unique<ShaderModule>(vkDevice, path));
    shaderModulePathToId.emplace(std::pair{path, modId});

    return modId;
  }

  static void validate_program_shaders(std::string_view name, const std::vector<vk::ShaderStageFlagBits> &stages)
  {
    auto supportedShaders =
      vk::ShaderStageFlagBits::eVertex|
      vk::ShaderStageFlagBits::eTessellationControl|
      vk::ShaderStageFlagBits::eTessellationEvaluation|
      vk::ShaderStageFlagBits::eGeometry|
      vk::ShaderStageFlagBits::eFragment|
      vk::ShaderStageFlagBits::eCompute;

    bool isComputePipeline = false;
    vk::ShaderStageFlags usageMask = static_cast<vk::ShaderStageFlags>(0);

    for (auto stage : stages)
    {
      ETNA_ASSERTF(stage & supportedShaders, "Shader program '{}' creation error, unsupported shader stage {}", name, vk::to_string(stage));
      ETNA_ASSERTF(!(stage & usageMask), "Shader program '{}' creation error, multiple files with {} shader stage", name, vk::to_string(stage));

      isComputePipeline |= (stage == vk::ShaderStageFlagBits::eCompute);
      usageMask |= stage;
    }

    ETNA_ASSERTF(!isComputePipeline || stages.size() == 1, "Shader program '{}' creating error, usage of compute shader with other stages", name);
  }

  ShaderProgramId ShaderProgramManager::loadProgram(std::string_view name, std::span<const std::filesystem::path> shaders_path)
  {
    ETNA_ASSERTF(!programNameToId.contains(name), "Shader program '{}' redefenition", name);

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

    auto progId = static_cast<ShaderProgramId>(programById.size());

    programById.emplace_back(new ShaderProgramInternal {name, std::move(moduleIds)});
    programById.back()->reload(*this);

    programNameToId.emplace(std::pair{std::string{name}, progId});

    return progId;
  }

  ShaderProgramId ShaderProgramManager::getProgram(std::string_view name) const
  {
    auto it = programNameToId.find(name);
    ETNA_ASSERTF(it != programNameToId.end(), "Shader program '{}' not found", name);
    return it->second;
  }

  void ShaderProgramManager::ShaderProgramInternal::reload(ShaderProgramManager &manager)
  {
    progLayout = {};
    usedDescriptors = {};
    pushConst = vk::PushConstantRange {};

    std::array<DescriptorSetInfo, MAX_PROGRAM_DESCRIPTORS> dstDescriptors;

    for (auto id : moduleIds)
    {
      auto &shaderMod = manager.getModule(id);

      if (shaderMod.getPushConst().size) //merge push constants
      {
        auto modPushConst = shaderMod.getPushConst();
        if (pushConst.size == 0u)
        {
          pushConst = modPushConst;
        }
        else
        {
          ETNA_ASSERTF(pushConst.size == modPushConst.size, "ShaderProgram '{}' : not compatible push constant blocks", name);
          pushConst.stageFlags |= modPushConst.stageFlags;
        }
      }

      const auto &resources = shaderMod.getResources(); //merge descriptors
      for (auto &desc : resources)
      {
        ETNA_ASSERTF(desc.first < MAX_PROGRAM_DESCRIPTORS, "ShaderProgram '{}' : set {} out of max sets ({})",
          name, desc.first, MAX_PROGRAM_DESCRIPTORS);

        usedDescriptors.set(desc.first);
        dstDescriptors[desc.first].merge(desc.second);
      }
    }

    std::vector<vk::DescriptorSetLayout> vkLayouts;

    for (uint32_t i = 0; i < MAX_PROGRAM_DESCRIPTORS; i++)
    {
      if (!usedDescriptors.test(i))
        continue;
      auto res = manager.descriptorSetLayoutCache.get(manager.vkDevice, dstDescriptors[i]);
      descriptorIds[i] = res.first;
      vkLayouts.push_back(res.second);
    }

    vk::PipelineLayoutCreateInfo info {};
    info.setSetLayouts(vkLayouts);

    if (pushConst.size)
    {
      info.setPPushConstantRanges(&pushConst);
      info.setPushConstantRangeCount(1u);
    }

    progLayout = manager.vkDevice.createPipelineLayoutUnique(info).value;
  }

  void ShaderProgramManager::logProgramInfo(const std::string& name) const
  {
    spdlog::info("Info for shader program '{}':", name);

    auto info = getProgramInfo(name);

    for (uint32_t set = 0u; set < etna::MAX_PROGRAM_DESCRIPTORS; set++)
    {
      if (!info.isDescriptorSetUsed(set))
        continue;
      auto setInfo = info.getDescriptorSetInfo(set);
      for (uint32_t binding = 0; binding < etna::MAX_DESCRIPTOR_BINDINGS; binding++)
      {
        if (!setInfo.isBindingUsed(binding))
          continue;
        auto &vkBinding = setInfo.getBinding(binding);

        spdlog::info("    Binding {}: type {}, stages {}, count {}",
          binding, vk::to_string(vkBinding.descriptorType),
          vkBinding.descriptorCount, vk::to_string(vkBinding.stageFlags));
      }
    }

    auto pc = info.getPushConst();
    if (pc.size)
    {
      spdlog::info("    Push constant block: size {}, stages {}", pc.size, vk::to_string(pc.stageFlags));
    }
  }

  void ShaderProgramManager::reloadPrograms()
  {
    for (auto &mod : shaderModules)
    {
      mod->reload(vkDevice);
    }

    for (auto &progPtr : programById)
    {
      progPtr->reload(*this);
    }
  }

  void ShaderProgramManager::clear()
  {
    programNameToId.clear();
    programById.clear();
    shaderModulePathToId.clear();
    shaderModules.clear();
  }

  std::vector<vk::PipelineShaderStageCreateInfo> ShaderProgramManager::getShaderStages(ShaderProgramId id) const
  {
    auto &prog = *programById.at(id);

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    stages.reserve(prog.moduleIds.size());

    for (auto id : prog.moduleIds)
    {
      const auto &shaderMod = getModule(id);
      vk::PipelineShaderStageCreateInfo info {};
      info.setModule(shaderMod.getVkModule());
      info.setStage(shaderMod.getStage());
      info.setPName(shaderMod.getNameCstr());
      stages.push_back(info);
    }
    return stages;
  }

  vk::DescriptorSetLayout ShaderProgramManager::getDescriptorLayout(ShaderProgramId id, uint32_t set) const
  {
    auto layoutId = getDescriptorLayoutId(id, set);
    return descriptorSetLayoutCache.getVkLayout(layoutId);
  }

  vk::PushConstantRange ShaderProgramInfo::getPushConst() const
  {
    auto &prog = mgr.getProgInternal(id);
    return prog.pushConst;
  }

  vk::PipelineLayout ShaderProgramInfo::getPipelineLayout() const
  {
    auto &prog = mgr.getProgInternal(id);
    return prog.progLayout.get();
  }

  bool ShaderProgramInfo::isDescriptorSetUsed(uint32_t set) const
  {
    auto &prog = mgr.getProgInternal(id);
    return set < MAX_PROGRAM_DESCRIPTORS && prog.usedDescriptors.test(set);
  }

  DescriptorLayoutId ShaderProgramInfo::getDescriptorLayoutId(uint32_t set) const
  {
    ETNA_ASSERT(isDescriptorSetUsed(set));
    return mgr.getProgInternal(id).descriptorIds.at(set);
  }

  vk::DescriptorSetLayout ShaderProgramInfo::getDescriptorSetLayout(uint32_t set) const
  {
    return  mgr.descriptorSetLayoutCache.getVkLayout(getDescriptorLayoutId(set));
  }

  const DescriptorSetInfo &ShaderProgramInfo::getDescriptorSetInfo(uint32_t set) const
  {
    return mgr.descriptorSetLayoutCache.getLayoutInfo(getDescriptorLayoutId(set));
  }
}
