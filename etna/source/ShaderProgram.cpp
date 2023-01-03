#include <etna/GlobalContext.hpp>
#include <etna/Vulkan.hpp>

#include <stdexcept>
#include <fstream>

#include <spirv_reflect.h>

namespace etna
{
  ShaderModule::ShaderModule(vk::Device device, const std::string &shader_path)
    : path {shader_path}
  {
    reload(device);
  }

  static std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      ETNA_PANIC("Failed to open file {}", filename);
    }

    size_t fileSize = (size_t) file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
  }

  struct SpvModDeleter
  {
    SpvModDeleter() {}
    void operator()(SpvReflectShaderModule *mod) const
    {
      spvReflectDestroyShaderModule(mod);
    }
  };

  #define SPRV_ASSERT(res, ...) if ((res) != SPV_REFLECT_RESULT_SUCCESS) ETNA_PANIC("SPIRV parse error", __VA_ARGS__)

  void ShaderModule::reload(vk::Device device)
  {
    vkModule = {};

    auto code = read_file(path);
    vk::ShaderModuleCreateInfo info {};
    info.setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    info.setCodeSize(code.size());

    if (code.size() % 4 != 0)
      ETNA_PANIC("SPIRV ", path, " broken");

    vkModule = device.createShaderModuleUnique(info).value;

    std::unique_ptr<SpvReflectShaderModule, SpvModDeleter> spvModule;
    spvModule.reset(new SpvReflectShaderModule {});

    SPRV_ASSERT(spvReflectCreateShaderModule(code.size(), code.data(), spvModule.get()), path);

    stage = static_cast<vk::ShaderStageFlagBits>(spvModule->shader_stage);
    entryPoint = spvModule->entry_point_name;

    resources.clear();

    uint32_t count = 0;
    SPRV_ASSERT(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, nullptr), path);

    std::vector<SpvReflectDescriptorSet*> sets(count);
    SPRV_ASSERT(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, sets.data()), path);

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
      if (blk.offset != 0) {
        ETNA_PANIC("SPIRV ", path, " parse error: PushConst offset is not zero");
      }
      pushConst.stageFlags = stage;
      pushConst.offset = 0u;
      pushConst.size = blk.size;
    }
    else if (spvModule->push_constant_block_count > 1)
    {
      ETNA_PANIC("SPIRV ", path, " parse error: only 1 push_const block per shader supported");
    }
    else
    {
      pushConst.size = 0u;
      pushConst.offset = 0u;
      pushConst.stageFlags = static_cast<vk::ShaderStageFlags>(0u);
    }
  }

  uint32_t ShaderProgramManager::registerModule(const std::string &path)
  {
    auto it = shaderModuleNames.find(path);
    if (it != shaderModuleNames.end())
      return it->second;

    uint32_t modId = static_cast<uint32_t>(shaderModules.size());
    std::unique_ptr<ShaderModule> newMod;
    newMod.reset(new ShaderModule {get_context().getDevice(), path}); 
    shaderModules.push_back(std::move(newMod));
    return modId;
  }

  static void validate_program_shaders(std::string_view name, const std::vector<vk::ShaderStageFlagBits> &stages) {
    auto supportedShaders =
      vk::ShaderStageFlagBits::eVertex|
      vk::ShaderStageFlagBits::eTessellationControl|
      vk::ShaderStageFlagBits::eTessellationEvaluation|
      vk::ShaderStageFlagBits::eGeometry|
      vk::ShaderStageFlagBits::eFragment|
      vk::ShaderStageFlagBits::eCompute;

    bool isComputePipeline = false;
    vk::ShaderStageFlags usageMask = static_cast<vk::ShaderStageFlags>(0);

    for (auto stage : stages) {
      if (!(stage & supportedShaders)) {
        ETNA_PANIC("Shader program ", name, " creating error, unsupported shader stage ", vk::to_string(stage));
      }

      if (stage & usageMask) {
        ETNA_PANIC("Shader program ", name, " creating error, multiple usage of", vk::to_string(stage), " shader stage");
      }

      isComputePipeline |= (stage == vk::ShaderStageFlagBits::eCompute);
      usageMask |= stage;
    }

    if (isComputePipeline && stages.size() != 1) {
      ETNA_PANIC("Shader program ", name, " creating error, usage of compute shader with other stages");
    }
  }

  ShaderProgramId ShaderProgramManager::loadProgram(std::string_view name, const std::vector<std::string> &shaders_path)
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
    auto &descriptorLayoutCache = get_context().getDescriptorSetLayouts();

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
        else {
          if (pushConst.size != modPushConst.size)
            ETNA_PANIC("ShaderProgram ", name, " : not compatible push constant blocks");
          pushConst.stageFlags |= modPushConst.stageFlags;
        }
      }

      const auto &resources = shaderMod.getResources(); //merge descriptors
      for (auto &desc : resources)
      {
        if (desc.first >= MAX_PROGRAM_DESCRIPTORS)
          ETNA_PANIC("ShaderProgram ", name, " : set ", desc.first, " out of max sets (", MAX_PROGRAM_DESCRIPTORS, ")");

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

    vk::PipelineLayoutCreateInfo info {};
    info.setSetLayouts(vkLayouts);

    if (pushConst.size)
    {
      info.setPPushConstantRanges(&pushConst);
      info.setPushConstantRangeCount(1u);
    }

    progLayout = get_context().getDevice().createPipelineLayoutUnique(info).value;
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
      mod->reload(get_context().getDevice());
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
    shaderModuleNames.clear();
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
    return get_context().getDescriptorSetLayouts().getVkLayout(layoutId);
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
    return  get_context().getDescriptorSetLayouts().getVkLayout(getDescriptorLayoutId(set));
  }
  
  const DescriptorSetInfo &ShaderProgramInfo::getDescriptorSetInfo(uint32_t set) const
  {
    return get_context().getDescriptorSetLayouts().getLayoutInfo(getDescriptorLayoutId(set));
  }

}
