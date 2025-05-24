#pragma once
#ifndef ETNA_DESCRIPTOR_SET_LAYOUT_HPP_INCLUDED
#define ETNA_DESCRIPTOR_SET_LAYOUT_HPP_INCLUDED

#include <array>
#include <bitset>
#include <vector>
#include <unordered_map>

#include <etna/Vulkan.hpp>


struct SpvReflectDescriptorSet;

namespace etna
{

constexpr uint32_t MAX_PROGRAM_DESCRIPTORS = 4u;
constexpr uint32_t MAX_DESCRIPTOR_BINDINGS =
  32u; /*If you are out of bindings, try using arrays of images/samplers*/

struct DescriptorSetLayoutHash;

struct DescriptorSetInfo
{
  void parseShader(vk::ShaderStageFlagBits stage, const SpvReflectDescriptorSet& spv);
  void addResource(
    const vk::DescriptorSetLayoutBinding& binding, vk::DescriptorBindingFlags flags = {});
  void merge(const DescriptorSetInfo& info);

  bool operator==(const DescriptorSetInfo& rhs) const;

  vk::DescriptorSetLayout createVkLayout(vk::Device device) const;

  void clear();

  bool isBindingUsed(uint32_t binding) const
  {
    return binding < usedBindingsCap && usedBindings.test(binding);
  }

  uint32_t getMaxBinding() const
  {
    ETNA_VERIFY(usedBindingsCap > 0);
    return usedBindingsCap - 1;
  }

  const vk::DescriptorSetLayoutBinding& getBinding(uint32_t binding) const
  {
    ETNA_VERIFY(isBindingUsed(binding));
    return bindings.at(binding);
  }

  vk::DescriptorBindingFlags getBindingFlags(uint32_t binding) const
  {
    ETNA_VERIFY(isBindingUsed(binding));
    return bindingFlags.at(binding);
  }

  bool hasDynamicDescriptorArray() const { return hasDynDescriptorArray; }
  uint32_t getDynamicDescriptorArraySizeCap() const
  {
    ETNA_VERIFY(hasDynamicDescriptorArray());
    return getBinding(getMaxBinding()).descriptorCount;
  }

private:
  uint32_t usedBindingsCap = 0;
  uint32_t dynOffsets = 0;

  std::bitset<MAX_DESCRIPTOR_BINDINGS> usedBindings{};
  std::array<vk::DescriptorSetLayoutBinding, MAX_DESCRIPTOR_BINDINGS> bindings{};
  std::array<vk::DescriptorBindingFlags, MAX_DESCRIPTOR_BINDINGS> bindingFlags{};

  // If this is true, the array is guaranteed to be in the usedBindingsCap - 1 slot
  bool hasDynDescriptorArray = false;

  friend DescriptorSetLayoutHash;
};

struct DescriptorSetLayoutHash
{
  std::size_t operator()(const DescriptorSetInfo& res) const;
};

using DescriptorLayoutId = uint32_t;

struct DescriptorSetLayoutCache
{
  DescriptorSetLayoutCache() {}
  ~DescriptorSetLayoutCache()
  {
    // make device global and call clear hear
  }

  DescriptorLayoutId registerLayout(vk::Device device, const DescriptorSetInfo& info);
  void clear(vk::Device device);

  const DescriptorSetInfo& getLayoutInfo(DescriptorLayoutId id) const { return descriptors.at(id); }

  vk::DescriptorSetLayout getVkLayout(DescriptorLayoutId id) const { return vkLayouts.at(id); }

  std::pair<DescriptorLayoutId, vk::DescriptorSetLayout> get(
    vk::Device device, const DescriptorSetInfo& info);

  DescriptorSetLayoutCache(const DescriptorSetLayoutCache&) = delete;
  DescriptorSetLayoutCache& operator=(const DescriptorSetLayoutCache&) = delete;

private:
  std::unordered_map<DescriptorSetInfo, DescriptorLayoutId, DescriptorSetLayoutHash> map;
  std::vector<DescriptorSetInfo> descriptors;
  std::vector<vk::DescriptorSetLayout> vkLayouts;
};

} // namespace etna

#endif // ETNA_DESCRIPTOR_SET_LAYOUT_HPP_INCLUDED
