#include <etna/ShaderProgram.hpp>
#include <etna/Error.hpp>

#include <stdexcept>
#include <fstream>

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
      ETNA_RUNTIME_ERROR("Failed to open file ", filename);
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

  #define SPRV_ASSERT(res, ...) if ((res) != SPV_REFLECT_RESULT_SUCCESS) ETNA_RUNTIME_ERROR("SPIRV parse error", __VA_ARGS__)

  void ShaderModule::reload(vk::Device device)
  {
    reset(device);

    auto code = read_file(path);
    vk::ShaderModuleCreateInfo info {};
    info.setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    info.setCodeSize(code.size());

    if (code.size() % 4 != 0)
      ETNA_RUNTIME_ERROR("SPIRV ", path, " broken");

    vkModule = device.createShaderModule(info);

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
        ETNA_RUNTIME_ERROR("SPIRV ", path, " parse error: PushConst offset is not zero");
      }
      pushConst.stageFlags = stage;
      pushConst.offset = 0u;
      pushConst.size = blk.size;
    }
    else if (spvModule->push_constant_block_count > 1)
    {
      ETNA_RUNTIME_ERROR("SPIRV ", path, " parse error: only 1 push_const block per shader supported");
    }
    else
    {
      pushConst.size = 0u;
      pushConst.offset = 0u;
      pushConst.stageFlags = static_cast<vk::ShaderStageFlags>(0u);
    }
  }
  
  void ShaderModule::reset(vk::Device device)
  {
    if (vkModule)
      device.destroyShaderModule(vkModule);
    vkModule = nullptr;
  }

  uint32_t ShaderProgramManager::registerModule(const std::string &path)
  {
    auto it = shaderModuleNames.find(path);
    if (it != shaderModuleNames.end())
      return it->second;

    uint32_t modId = shaderModules.size();
    std::unique_ptr<ShaderModule> newMod;
    newMod.reset(new ShaderModule {getDevice(), path}); 
    shaderModules.push_back(std::move(newMod));
    return modId;
  }

  static void validate_program_shaders(const std::string &name, const std::vector<vk::ShaderStageFlagBits> &stages) {
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
        ETNA_RUNTIME_ERROR("Shader program ", name, " creating error, unsupported shader stage ", vk::to_string(stage));
      }

      if (stage & usageMask) {
        ETNA_RUNTIME_ERROR("Shader program ", name, " creating error, multiple usage of", vk::to_string(stage), " shader stage");
      }

      isComputePipeline |= (stage == vk::ShaderStageFlagBits::eCompute);
      usageMask |= stage;
    }

    if (isComputePipeline && stages.size() != 1) {
      ETNA_RUNTIME_ERROR("Shader program ", name, " creating error, usage of compute shader with other stages");
    }
  }

  ShaderProgramId ShaderProgramManager::loadProgram(const std::string &name, const std::vector<std::string> &shaders_path)
  {
    if (programNames.find(name) != programNames.end())
      ETNA_RUNTIME_ERROR("Shader program ", name, " redefenition");
    
    std::vector<uint32_t> moduleIds;
    std::vector<vk::ShaderStageFlagBits> stages;
    for (const auto &path : shaders_path)
    {
      auto id = registerModule(path);
      auto stage = getModule(id).getStage();

      moduleIds.push_back(id);
      stages.push_back(stage);
    }

    validate_program_shaders(name, stages);

    ShaderProgramId progId = programs.size();
    programs.emplace_back(new ShaderProgramInternal {name, std::move(moduleIds)});
    programs[progId]->reload(*this);
    programNames[name] = progId;
    return progId;
  }
  
  ShaderProgramId ShaderProgramManager::getProgram(const std::string &name) const
  {
    auto it = programNames.find(name);
    if (it == programNames.end())
      ETNA_RUNTIME_ERROR("Shader program ", name, " not found");
    return it->second;
  }

  void ShaderProgramManager::ShaderProgramInternal::reload(ShaderProgramManager &manager)
  {
    destroy(manager);

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
        else {
          if (pushConst.size != modPushConst.size)
            ETNA_RUNTIME_ERROR("ShaderProgram ", name, " : not compatible push constant blocks");
          pushConst.stageFlags |= modPushConst.stageFlags;
        }
      }

      const auto &resources = shaderMod.getResources(); //merge descriptors
      for (auto &desc : resources)
      {
        if (desc.first >= MAX_PROGRAM_DESCRIPTORS)
          ETNA_RUNTIME_ERROR("ShaderProgram ", name, " : set ", desc.first, " out of max sets (", MAX_PROGRAM_DESCRIPTORS, ")");

        usedDescriptors.set(desc.first);
        dstDescriptors[desc.first].merge(desc.second);
      }
    }

    std::vector<vk::DescriptorSetLayout> vkLayouts;

    for (uint32_t i = 0; i < MAX_PROGRAM_DESCRIPTORS; i++)
    {
      if (!usedDescriptors.test(i))
        continue;
      auto res = manager.descriptorLayoutCache.get(manager.getDevice(), dstDescriptors[i]);
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

    progLayout = manager.getDevice().createPipelineLayout(info);
  }
  
  void ShaderProgramManager::ShaderProgramInternal::destroy(ShaderProgramManager &manager)
  {
    if (progLayout)
      manager.getDevice().destroyPipelineLayout(progLayout);
    progLayout = nullptr;

    usedDescriptors.reset();
    pushConst = vk::PushConstantRange {};
  }

  void ShaderProgramManager::reloadPrograms()
  {
    descriptorLayoutCache.clear(getDevice());

    for (auto &mod : shaderModules)
    {
      mod->reload(getDevice());
    }

    for (auto &progPtr : programs)
    {
      progPtr->reload(*this);
    }
  }
  
  void ShaderProgramManager::clear()
  {
    programNames.clear();
    for (auto &progPtr : programs)
      progPtr->destroy(*this);
    programs.clear();

    shaderModuleNames.clear();
    for (auto &modPtr : shaderModules)
      modPtr->reset(getDevice());
    shaderModules.clear();
    
    descriptorLayoutCache.clear(getDevice());
  }

  std::vector<vk::PipelineShaderStageCreateInfo> ShaderProgramManager::getShaderStages(ShaderProgramId id) const
  {
    auto &prog = *programs.at(id);

    std::vector<vk::PipelineShaderStageCreateInfo> stages;
    stages.reserve(prog.moduleIds.size());

    for (auto id : prog.moduleIds)
    {
      const auto &shaderMod = getModule(id);
      vk::PipelineShaderStageCreateInfo info {};
      info.setModule(shaderMod.getVkModule());
      info.setStage(shaderMod.getStage());
      info.setPName(shaderMod.getName().c_str());
      stages.push_back(info);
    }
    return stages;
  }

  vk::PushConstantRange ShaderProgramInfo::getPushConst() const
  {
    auto &prog = mgr.getProgInternal(id);
    return prog.pushConst;
  }
  
  vk::PipelineLayout ShaderProgramInfo::getPipelineLayout() const
  {
    auto &prog = mgr.getProgInternal(id);
    return prog.progLayout;
  }

  bool ShaderProgramInfo::isDescriptorSetUsed(uint32_t set) const
  {
    auto &prog = mgr.getProgInternal(id);
    return set < MAX_PROGRAM_DESCRIPTORS && prog.usedDescriptors.test(set);
  }
  
  vk::DescriptorSetLayout ShaderProgramInfo::getDescriptorSetLayout(uint32_t set) const
  {
    ETNA_ASSERT(isDescriptorSetUsed(set));
    auto did = mgr.getProgInternal(id).descriptorIds.at(set);
    return mgr.descriptorLayoutCache.getVkLayout(did);
  }
  
  const DescriptorSetInfo &ShaderProgramInfo::getDescriptorSetInfo(uint32_t set) const
  {
    ETNA_ASSERT(isDescriptorSetUsed(set));
    auto did = mgr.getProgInternal(id).descriptorIds.at(set);
    return mgr.descriptorLayoutCache.getLayoutInfo(did);
  }

}
