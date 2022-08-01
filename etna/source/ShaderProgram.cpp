#include <etna/ShaderProgram.hpp>

#include <stdexcept>
#include <fstream>

namespace etna
{
  void DescriptorSetInfo::addResource(const vk::DescriptorSetLayoutBinding &binding)
  {
    if (binding.binding > MAX_DESCRIPTOR_BINDINGS)
      throw std::runtime_error {"DescriptorSetInfo: Binding out of MAX_DESCRIPTOR_BINDINGS range"};

    if (usedBindings.test(binding.binding))
    {
      auto &src = bindings[binding.binding];
      if (src.descriptorType != binding.descriptorType
        || src.descriptorCount != binding.descriptorCount)
      {
        throw std::runtime_error {"DescriptorSetInfo: incompatible bindings"};
      }

      src.stageFlags |= binding.stageFlags;
      return;
    }
    
    usedBindings.set(binding.binding);
    bindings[binding.binding] = binding;

    if (binding.binding + 1 > maxUsedBinding)
      maxUsedBinding = binding.binding + 1;
    if (binding.descriptorType == vk::DescriptorType::eUniformBufferDynamic || binding.descriptorType == vk::DescriptorType::eStorageBufferDynamic)
      dynOffsets++;
  }
  
  void DescriptorSetInfo::clear()
  {
    maxUsedBinding = 0;
    dynOffsets = 0;
    usedBindings.reset();
    for (auto &binding : bindings)
      binding = vk::DescriptorSetLayoutBinding {};
  }

  void DescriptorSetInfo::parseShader(vk::ShaderStageFlagBits stage, const SpvReflectDescriptorSet &spv)
  {
    for (uint32_t i = 0u; i < spv.binding_count; i++)
    {
      const auto &spvBinding = *spv.bindings[i];
      
      vk::DescriptorSetLayoutBinding apiBinding {};
      apiBinding.descriptorCount = 1;

      for (uint32_t i = 0; i < spvBinding.array.dims_count; i++) {
        apiBinding.descriptorCount *= spvBinding.array.dims[i];
      }

      apiBinding.descriptorType = static_cast<vk::DescriptorType>(spvBinding.descriptor_type);
      apiBinding.stageFlags = stage;
      apiBinding.pImmutableSamplers = nullptr;
      apiBinding.binding = spvBinding.binding;
      addResource(apiBinding);
    }
  }
  
  void DescriptorSetInfo::merge(const DescriptorSetInfo &info)
  {
    for (uint32_t binding = 0; binding < info.maxUsedBinding; binding++)
    {
      if (!info.usedBindings.test(binding))
        continue;
      addResource(info.bindings[binding]);
    }
  }

  bool DescriptorSetInfo::operator==(const DescriptorSetInfo &rhs) const
  {
    if (maxUsedBinding != rhs.maxUsedBinding)
      return false;
    if (usedBindings != rhs.usedBindings)
      return false;
    
    for (uint32_t i = 0; i < maxUsedBinding; i++)
    {
      if (bindings[i] != rhs.bindings[i])
        return false;
    }

    return true;
  }

  vk::DescriptorSetLayout DescriptorSetInfo::createVkLayout(vk::Device device) const
  {
    std::vector<vk::DescriptorSetLayoutBinding> apiBindings;
    for (uint32_t i = 0; i < maxUsedBinding; i++)
    {
      if (!usedBindings.test(i))
        continue;
      apiBindings.push_back(bindings[i]);
    }

    vk::DescriptorSetLayoutCreateInfo info {};
    info.setBindings(apiBindings);
    
    return device.createDescriptorSetLayout(info);
  }

  template <typename T>
  inline void hash_combine(std::size_t &s, const T &v)
  {
    std::hash<T> h;
    s ^= h(v) + 0x9e3779b9 + (s<< 6) + (s>> 2); 
  }

  std::size_t DescriptorSetLayoutHash::operator()(const DescriptorSetInfo &res) const
  {
    size_t hash = 0;

    for (uint32_t i = 0; i < res.maxUsedBinding; i++)
    {
      if (!res.usedBindings.test(i))
        continue;
        
      hash_combine(hash, res.bindings[i].binding);
      hash_combine(hash, static_cast<uint32_t>(res.bindings[i].descriptorType));
      hash_combine(hash, res.bindings[i].descriptorCount);
      hash_combine(hash, static_cast<uint32_t>(res.bindings[i].stageFlags));
    }

    return hash;
  }

  DescriptorLayoutId DescriptorSetLayoutCache::registerLayout(vk::Device device, const DescriptorSetInfo &info)
  {
    auto it = map.find(info);
    if (it != map.end())
      return it->second;
    
    DescriptorLayoutId id = descriptors.size();
    map.insert({info, id});
    descriptors.push_back(info);
    vkLayouts.push_back(info.createVkLayout(device));
    return id;
  }
  
  void DescriptorSetLayoutCache::clear(vk::Device device)
  {
    for (auto layout : vkLayouts) {
      device.destroyDescriptorSetLayout(layout);
    }

    map.clear();
    descriptors.clear();
    vkLayouts.clear();
  }

  ShaderModule::ShaderModule(vk::Device device, const std::string &shader_path)
    : path {shader_path}
  {
    reload(device);
  }

  static std::vector<char> read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
      std::stringstream ss;
      ss << "Failed to open file " << filename;
      throw std::runtime_error {ss.str()};
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

  #define SPVR_ASSER(res) if ((res) != SPV_REFLECT_RESULT_SUCCESS) throw std::runtime_error {"SPVReflect error"}

  void ShaderModule::reload(vk::Device device)
  {
    reset(device);

    auto code = read_file(path);
    vk::ShaderModuleCreateInfo info {};
    info.setPCode(reinterpret_cast<const uint32_t*>(code.data()));
    info.setCodeSize(code.size());
   
    vkModule = device.createShaderModule(info);

    std::unique_ptr<SpvReflectShaderModule, SpvModDeleter> spvModule;
    spvModule.reset(new SpvReflectShaderModule {});

    if (spvReflectCreateShaderModule(code.size(), code.data(), spvModule.get()) != SPV_REFLECT_RESULT_SUCCESS) {
      throw std::runtime_error {"Shader parsing error"};
    }

    stage = static_cast<vk::ShaderStageFlagBits>(spvModule->shader_stage);
    entryPoint = spvModule->entry_point_name;

    resources.clear();

    uint32_t count = 0;
    SPVR_ASSER(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, nullptr));

    std::vector<SpvReflectDescriptorSet*> sets(count);
    SPVR_ASSER(spvReflectEnumerateDescriptorSets(spvModule.get(), &count, sets.data()));

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
        throw std::runtime_error {"PushConst offset n e 0"};
      }
      pushConst.stageFlags = stage;
      pushConst.offset = 0u;
      pushConst.size = blk.size;
    }
    else if (spvModule->push_constant_block_count > 1)
    {
      throw std::runtime_error {"Shader parse error, only 1 push_const block per shader supported"};
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

  std::unique_ptr<ShaderProgramManager::ShaderProgramInternal> ShaderProgramManager::createProgram(std::vector<uint32_t> &&modules)
  {
    std::unique_ptr<ShaderProgramInternal> prog;
    prog.reset(new ShaderProgramInternal {});
    prog->moduleIds = std::move(modules);
    prog->usedDescriptors.reset();
    prog->pushConst = vk::PushConstantRange {};

    std::array<DescriptorSetInfo, MAX_PROGRAM_DESCRIPTORS> dstDescriptors;

    for (auto id : prog->moduleIds)
    {
      auto &shaderMod = *shaderModules[id];

      if (shaderMod.getPushConstUsed()) //merge push constants
      {
        auto pushConst = shaderMod.getPushConst();
        if (prog->pushConst.size == 0u)
        {
          prog->pushConst = pushConst;
        }
        else {
          if (prog->pushConst.size != pushConst.size)
            throw std::runtime_error {"ShaderProgram: not compatible push constant blocks"};
          prog->pushConst.stageFlags |= pushConst.stageFlags;
        }
      }

      const auto &resources = shaderMod.getResources(); //merge descriptors
      for (auto &desc : resources)
      {
        if (desc.first >= MAX_PROGRAM_DESCRIPTORS)
          throw std::runtime_error {"ShaderProgram: Descriptor set out of range"};

        prog->usedDescriptors.set(desc.first);
        dstDescriptors[desc.first].merge(desc.second);
      }
    }

    std::vector<vk::DescriptorSetLayout> vkLayouts;

    for (uint32_t i = 0; i < MAX_PROGRAM_DESCRIPTORS; i++)
    {
      if (!prog->usedDescriptors.test(i))
        continue;
      prog->descriptorIds[i] = desciptorLayoutCache.registerLayout(getDevice(), dstDescriptors[i]);
      vkLayouts.push_back(desciptorLayoutCache.getVkLayout(prog->descriptorIds[i]));
    }

    vk::PipelineLayoutCreateInfo info {};
    info.setSetLayouts(vkLayouts);

    if (prog->pushConst.size)
    {
      info.setPPushConstantRanges(&prog->pushConst);
      info.setPushConstantRangeCount(1u);
    }

    prog->progLayout = getDevice().createPipelineLayout(info);    

    return prog;
  }

  static void validate_program_shaders(const std::vector<vk::ShaderStageFlagBits> &stages) {
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
        throw std::runtime_error {"Error, unsupported shader"};
      }

      if (stage & usageMask) {
        throw std::runtime_error {"Error, multiple usage of same shader stage"};
      }

      isComputePipeline |= (stage == vk::ShaderStageFlagBits::eCompute);
      usageMask |= stage;
    }

    if (isComputePipeline && stages.size() != 1) {
      throw std::runtime_error {"Error, usage of compute shader with other stages"};
    }
  }

  ShaderProgramId ShaderProgramManager::loadProgram(const std::string &name, const std::vector<std::string> &shaders_path)
  {
    if (programNames.find(name) != programNames.end())
      throw std::runtime_error {"ShaderProgram: shader program redefinition"};
    
    std::vector<uint32_t> moduleIds;
    std::vector<vk::ShaderStageFlagBits> stages;
    for (const auto &path : shaders_path)
    {
      auto id = registerModule(path);
      auto stage = getModule(id).getStage();

      moduleIds.push_back(id);
      stages.push_back(stage);
    }

    validate_program_shaders(stages);

    ShaderProgramId progId = programs.size();
    programs.push_back(createProgram(std::move(moduleIds)));
    programNames[name] = progId;
    return progId;
  }
  
  ShaderProgramId ShaderProgramManager::getProgram(const std::string &name)
  {
    auto it = programNames.find(name);
    if (it == programNames.end())
      throw std::runtime_error {"ShaderProgram: program not found"};
    return it->second;
  }

  void ShaderProgramManager::destroyProgram(std::unique_ptr<ShaderProgramInternal> &ptr)
  {
    if (ptr->progLayout)
      getDevice().destroyPipelineLayout(ptr->progLayout);
    ptr.reset(nullptr);
  }

  void ShaderProgramManager::reloadPrograms()
  {
    desciptorLayoutCache.clear(getDevice());

    for (auto &mod : shaderModules)
    {
      mod->reload(getDevice());
    }

    for (auto &progPtr : programs)
    {
      auto moduleIds = std::move(progPtr->moduleIds);
      destroyProgram(progPtr);
      progPtr = createProgram(std::move(moduleIds));
    }
  }
  
  void ShaderProgramManager::clear()
  {
    programNames.clear();
    for (auto &progPtr : programs)
      destroyProgram(progPtr);
    programs.clear();

    shaderModuleNames.clear();
    for (auto &modPtr : shaderModules)
      modPtr->reset(getDevice());
    shaderModules.clear();
    
    desciptorLayoutCache.clear(getDevice());
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

}
