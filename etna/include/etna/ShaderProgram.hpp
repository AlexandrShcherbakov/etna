#pragma once
#ifndef ETNA_SHADER_PROGRAM_HPP_INCLUDED
#define ETNA_SHADER_PROGRAM_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/Forward.hpp>
#include <etna/DescriptorSetLayout.hpp>
#include <etna/detail/StringMap.hpp>

#include <array>
#include <bitset>
#include <vector>
#include <memory>
#include <filesystem>


namespace etna
{
  struct ShaderModule
  {
    ShaderModule(vk::Device device, std::filesystem::path shader_path);

    void reload(vk::Device device);

    const auto &getResources() const { return resources; }
    vk::ShaderModule getVkModule() const { return vkModule.get(); }
    vk::ShaderStageFlagBits getStage() const { return stage; }
    std::string_view getName() const { return entryPoint; }
    const char* getNameCstr() const { return entryPoint.c_str(); }
    vk::PushConstantRange getPushConst() const { return pushConst; }

  private:
    std::filesystem::path path;
    std::string entryPoint;
    vk::ShaderStageFlagBits stage;

    vk::UniqueShaderModule vkModule;
    std::vector<std::pair<uint32_t, DescriptorSetInfo>> resources; /*set index - set resources*/
    vk::PushConstantRange pushConst;
    /*Todo: add vertex input info*/
  };

  struct ShaderProgramInfo
  {
    ShaderProgramId getId() const { return id; }

    vk::PushConstantRange getPushConst() const;
    vk::PipelineLayout getPipelineLayout() const;

    bool isDescriptorSetUsed(uint32_t set) const;
    vk::DescriptorSetLayout getDescriptorSetLayout(uint32_t set) const;
    DescriptorLayoutId getDescriptorLayoutId(uint32_t set) const;
    const DescriptorSetInfo &getDescriptorSetInfo(uint32_t set) const;

  private:
    ShaderProgramInfo(const ShaderProgramManager &manager, ShaderProgramId prog_id)
      : mgr {manager}, id {prog_id} {}

    const ShaderProgramManager &mgr;
    ShaderProgramId id;

    friend ShaderProgramManager;
  };

  struct DescriptorSetLayoutCache;

  struct ShaderProgramManager
  {
    ShaderProgramManager(vk::Device dev, DescriptorSetLayoutCache& descSetLayoutCache)
      : vkDevice{dev}, descriptorSetLayoutCache{descSetLayoutCache}
    {
    }

    ~ShaderProgramManager() { clear(); }

    ShaderProgramId loadProgram(std::string_view name, std::span<const std::filesystem::path> shaders_path);
    ShaderProgramId getProgram(std::string_view name) const;

    ShaderProgramInfo getProgramInfo(ShaderProgramId id) const
    {
      return ShaderProgramInfo {*this, id};
    }

    ShaderProgramInfo getProgramInfo(std::string_view name) const
    {
      return getProgramInfo(getProgram(name));
    }

    void logProgramInfo(const std::string& name) const;

    void reloadPrograms();
    void clear();

    vk::PipelineLayout getProgramLayout(ShaderProgramId id) const { return programById.at(id)->progLayout.get(); }
    vk::DescriptorSetLayout getDescriptorLayout(ShaderProgramId id, uint32_t set) const;

    DescriptorLayoutId getDescriptorLayoutId(ShaderProgramId id, uint32_t set) const
    {
      auto &prog = *programById.at(id);
      if (set >= MAX_PROGRAM_DESCRIPTORS || !prog.usedDescriptors.test(set))
        ETNA_PANIC("ShaderProgram ", prog.name, " invalid descriptor set #", set);
      return prog.descriptorIds[set];
    }

    std::vector<vk::PipelineShaderStageCreateInfo> getShaderStages(ShaderProgramId id) const; /*for pipeline creation*/

    ShaderProgramManager(const ShaderProgramManager&) = delete;
    ShaderProgramManager &operator=(const ShaderProgramManager &) = delete;

  private:
    vk::Device vkDevice;
    DescriptorSetLayoutCache& descriptorSetLayoutCache;

    std::unordered_map<std::filesystem::path, uint32_t> shaderModulePathToId;
    std::vector<std::unique_ptr<ShaderModule>> shaderModules;

    uint32_t registerModule(const std::filesystem::path& path);
    const ShaderModule &getModule(uint32_t id) const { return *shaderModules.at(id); }

    struct ShaderProgramInternal
    {
      ShaderProgramInternal(std::string_view name_, std::vector<uint32_t> &&mod) : name{name_}, moduleIds{std::move(mod)} {}

      std::string name;
      std::vector<uint32_t> moduleIds;

      std::bitset<MAX_PROGRAM_DESCRIPTORS> usedDescriptors;
      std::array<DescriptorLayoutId, MAX_PROGRAM_DESCRIPTORS> descriptorIds;

      vk::PushConstantRange pushConst {};
      vk::UniquePipelineLayout progLayout;

      void reload(ShaderProgramManager &manager);
    };

    StringMap<uint32_t> programNameToId;
    std::vector<std::unique_ptr<ShaderProgramInternal>> programById;

    const ShaderProgramInternal &getProgInternal(ShaderProgramId id) const
    {
      return *programById.at(id);
    }

    friend ShaderProgramInfo;
  };
}

#endif // ETNA_SHADER_PROGRAM_HPP_INCLUDED
