#pragma once
#ifndef ETNA_SHADER_PROGRAM_HPP_INCLUDED
#define ETNA_SHADER_PROGRAM_HPP_INCLUDED

#include <vulkan/vulkan.hpp>
#include <SPIRV-Reflect/spirv_reflect.h>
#include <bitset>

#include <array>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <memory>

namespace etna
{
  constexpr uint32_t MAX_PROGRAM_DESCRIPTORS = 4u;
  constexpr uint32_t MAX_DESCRIPTOR_BINDINGS = 8u; /*If you are out of bindings, try using arrays of images/samplers*/
  
  struct DescriptorSetLayoutHash;

  struct DescriptorSetInfo
  {
    void parseShader(vk::ShaderStageFlagBits stage, const SpvReflectDescriptorSet &spv);
    void addResource(const vk::DescriptorSetLayoutBinding &binding);
    void merge(const DescriptorSetInfo &info);

    bool operator==(const DescriptorSetInfo &rhs) const;

    vk::DescriptorSetLayout createVkLayout(vk::Device device) const;

    void clear();
  private:
    uint32_t maxUsedBinding = 0;
    uint32_t dynOffsets = 0;

    std::bitset<MAX_DESCRIPTOR_BINDINGS> usedBindings {};
    std::array<vk::DescriptorSetLayoutBinding, MAX_DESCRIPTOR_BINDINGS> bindings {};

    friend DescriptorSetLayoutHash;
  };

  struct DescriptorSetLayoutHash
  {
    std::size_t operator()(const DescriptorSetInfo &res) const;
  };

  using DescriptorLayoutId = uint32_t;

  struct DescriptorSetLayoutCache
  {
    DescriptorSetLayoutCache() {}
    ~DescriptorSetLayoutCache() { /*make device global and call clear hear*/}
    
    DescriptorLayoutId registerLayout(vk::Device device, const DescriptorSetInfo &info);
    void clear(vk::Device device);

    const DescriptorSetInfo &getLayoutInfo(DescriptorLayoutId id) const
    {
      return descriptors.at(id);
    }

    VkDescriptorSetLayout getVkLayout(DescriptorLayoutId id) const
    {
      return vkLayouts.at(id);
    }

    DescriptorSetLayoutCache(const DescriptorSetLayoutCache&) = delete;
    DescriptorSetLayoutCache &operator=(const DescriptorSetLayoutCache&) = delete; 

  private:
    std::unordered_map<DescriptorSetInfo, DescriptorLayoutId, DescriptorSetLayoutHash> map;
    std::vector<DescriptorSetInfo> descriptors;
    std::vector<vk::DescriptorSetLayout> vkLayouts;
  };

  struct ShaderModule
  {
    ShaderModule(vk::Device device, const std::string &shader_path);

    void reload(vk::Device device);
    void reset(vk::Device device);

    const auto &getResources() const { return resources; }
    vk::ShaderModule getVkModule() const { return vkModule; }
    vk::ShaderStageFlagBits getStage() const { return stage; }
    const std::string &getName() const { return entryPoint; }
    vk::PushConstantRange getPushConst() const { return pushConst; }

    bool getPushConstUsed() const { return pushConst.size !=0u; }

    ShaderModule(const ShaderModule &mod) = delete;
    ShaderModule &operator=(const ShaderModule &mod) = delete;  
  private:
    std::string path {};
    std::string entryPoint {};
    vk::ShaderStageFlagBits stage;

    vk::ShaderModule vkModule {nullptr};
    std::vector<std::pair<uint32_t, DescriptorSetInfo>> resources {}; /*set index - set resources*/
    vk::PushConstantRange pushConst {};
    /*Todo: add vertex input info*/
  };

  using ShaderProgramId = uint32_t;

  struct ShaderProgramManager
  {
    ShaderProgramManager(vk::Device device) : vkDevice {device} {}
    ~ShaderProgramManager() { clear(); }

    ShaderProgramId loadProgram(const std::string &name, const std::vector<std::string> &shaders_path);
    ShaderProgramId getProgram(const std::string &name);

    void reloadPrograms(); 
    void clear();

    vk::PipelineLayout getProgramLayout(ShaderProgramId id) const { return programs.at(id)->progLayout; } 
    vk::DescriptorSetLayout getDescriptorLayout(ShaderProgramId id, uint32_t set) const
    { 
      auto &prog = *programs.at(id);
      if (set >= MAX_PROGRAM_DESCRIPTORS || !prog.usedDescriptors.test(set))
        throw std::runtime_error {"ShaderProgram: invalid descriptor set index"};
      
      return desciptorLayoutCache.getVkLayout(prog.descriptorIds[set]);
    }

    std::vector<vk::PipelineShaderStageCreateInfo> getShaderStages(ShaderProgramId id) const; /*for pipeline creation*/

    ShaderProgramManager(const ShaderProgramManager&) = delete;
    ShaderProgramManager &operator=(const ShaderProgramManager &) = delete;

  private:
    vk::Device vkDevice {nullptr};
    
    DescriptorSetLayoutCache desciptorLayoutCache;

    std::unordered_map<std::string, uint32_t> shaderModuleNames;
    std::vector<std::unique_ptr<ShaderModule>> shaderModules;

    uint32_t registerModule(const std::string &path);
    const ShaderModule &getModule(uint32_t id) const { return *shaderModules.at(id); }

    struct ShaderProgramInternal
    {
      std::vector<uint32_t> moduleIds;

      std::bitset<MAX_PROGRAM_DESCRIPTORS> usedDescriptors;
      std::array<DescriptorLayoutId, MAX_PROGRAM_DESCRIPTORS> descriptorIds;
    
      vk::PushConstantRange pushConst {};
      vk::PipelineLayout progLayout {nullptr};
    };

    std::unordered_map<std::string, uint32_t> programNames;
    std::vector<std::unique_ptr<ShaderProgramInternal>> programs;

    std::unique_ptr<ShaderProgramInternal> createProgram(std::vector<uint32_t> &&modules);
    void destroyProgram(std::unique_ptr<ShaderProgramInternal> &ptr);
    //void reloadPrograms();

    vk::Device getDevice() const { return vkDevice; } /*make device global*/
  };
}

#endif