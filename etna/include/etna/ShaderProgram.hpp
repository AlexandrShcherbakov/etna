#pragma once
#ifndef ETNA_SHADER_PROGRAM_HPP_INCLUDED
#define ETNA_SHADER_PROGRAM_HPP_INCLUDED

#include <array>
#include <bitset>
#include <vector>
#include <unordered_map>
#include <memory>
#include <filesystem>

#include <etna/Vulkan.hpp>
#include <etna/Forward.hpp>
#include <etna/DescriptorSetLayout.hpp>


namespace etna
{

struct ShaderModule
{
  ShaderModule(vk::Device device, std::filesystem::path shader_path);

  void reload(vk::Device device);

  const auto& getResources() const { return resources; }
  vk::ShaderModule getVkModule() const { return vkModule.get(); }
  vk::ShaderStageFlagBits getStage() const { return stage; }
  const std::string& getName() const { return entryPoint; }
  vk::PushConstantRange getPushConst() const { return pushConst; }

  ShaderModule(const ShaderModule& mod) = delete;
  ShaderModule& operator=(const ShaderModule& mod) = delete;

private:
  std::filesystem::path path{};
  std::string entryPoint{};
  vk::ShaderStageFlagBits stage;

  vk::UniqueShaderModule vkModule;
  std::vector<std::pair<uint32_t, DescriptorSetInfo>> resources{}; /*set index - set resources*/
  vk::PushConstantRange pushConst{};
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
  const DescriptorSetInfo& getDescriptorSetInfo(uint32_t set) const;

private:
  ShaderProgramInfo(const ShaderProgramManager& manager, ShaderProgramId prog_id)
    : mgr{manager}
    , id{prog_id}
  {
  }

  const ShaderProgramManager& mgr;
  ShaderProgramId id;

  friend ShaderProgramManager;
};

struct ShaderProgramManager
{
  ShaderProgramManager() {}
  ~ShaderProgramManager() { clear(); }

  ShaderProgramId loadProgram(
    const char* name, std::span<std::filesystem::path const> shaders_path);
  ShaderProgramId tryGetProgram(const char* name) const;
  ShaderProgramId getProgram(const char* name) const;

  ShaderProgramInfo getProgramInfo(ShaderProgramId id) const
  {
    return ShaderProgramInfo{*this, id};
  }

  ShaderProgramInfo getProgramInfo(const char* name) const
  {
    return getProgramInfo(getProgram(name));
  }

  void reloadPrograms();
  void clear();

  vk::PipelineLayout getProgramLayout(ShaderProgramId id) const
  {
    return getProgInternal(id).progLayout.get();
  }
  vk::DescriptorSetLayout getDescriptorLayout(ShaderProgramId id, uint32_t set) const;

  DescriptorLayoutId getDescriptorLayoutId(ShaderProgramId id, uint32_t set) const
  {
    auto& prog = *programs.at(static_cast<std::underlying_type_t<ShaderProgramId>>(id));
    if (set >= MAX_PROGRAM_DESCRIPTORS || !prog.usedDescriptors.test(set))
      ETNA_PANIC("ShaderProgram {} invalid descriptor set #{}", prog.name, set);
    return prog.descriptorIds[set];
  }

  // for pipeline creation
  std::vector<vk::PipelineShaderStageCreateInfo> getShaderStages(ShaderProgramId id) const;

  ShaderProgramManager(const ShaderProgramManager&) = delete;
  ShaderProgramManager& operator=(const ShaderProgramManager&) = delete;

private:
  std::unordered_map<std::filesystem::path, uint32_t> shaderModuleNames;
  std::vector<std::unique_ptr<ShaderModule>> shaderModules;

  uint32_t registerModule(std::filesystem::path path);
  const ShaderModule& getModule(uint32_t id) const { return *shaderModules.at(id); }

  struct ShaderProgramInternal
  {
    ShaderProgramInternal(std::string in_name, std::vector<uint32_t>&& mod)
      : name(std::move(in_name))
      , moduleIds{std::move(mod)}
    {
    }

    std::string name;
    std::vector<uint32_t> moduleIds;

    std::bitset<MAX_PROGRAM_DESCRIPTORS> usedDescriptors;
    std::array<DescriptorLayoutId, MAX_PROGRAM_DESCRIPTORS> descriptorIds;

    vk::PushConstantRange pushConst{};
    vk::UniquePipelineLayout progLayout;

    void reload(ShaderProgramManager& manager);
  };

  std::unordered_map<std::string, ShaderProgramId> programNames;
  std::vector<std::unique_ptr<ShaderProgramInternal>> programs;

  const ShaderProgramInternal& getProgInternal(ShaderProgramId id) const
  {
    return *programs.at(static_cast<std::underlying_type_t<ShaderProgramId>>(id));
  }

  friend ShaderProgramInfo;
};

} // namespace etna

#endif // ETNA_SHADER_PROGRAM_HPP_INCLUDED
