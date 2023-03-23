#include <etna/DescriptorSetLayout.hpp>
#include <etna/Assert.hpp>

#include <spirv_reflect.h>

namespace etna
{
  void DescriptorSetInfo::addResource(const vk::DescriptorSetLayoutBinding &binding)
  {
    if (binding.binding > MAX_DESCRIPTOR_BINDINGS)
      ETNA_PANIC("DescriptorSetInfo: Binding {} out of MAX_DESCRIPTOR_BINDINGS range", binding.binding);

    if (usedBindings.test(binding.binding))
    {
      auto &src = bindings[binding.binding];
      if (src.descriptorType != binding.descriptorType
        || src.descriptorCount != binding.descriptorCount)
      {
        ETNA_PANIC("DescriptorSetInfo: incompatible bindings at index {}", binding.binding);
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
    
    return device.createDescriptorSetLayout(info).value;
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
    return get(device, info).first;
  }

  std::pair<DescriptorLayoutId, vk::DescriptorSetLayout> DescriptorSetLayoutCache::get(vk::Device device, const DescriptorSetInfo &info)
  {
    auto it = map.find(info);
    if (it != map.end())
      return {it->second, vkLayouts[it->second]};
    
    DescriptorLayoutId id = static_cast<DescriptorLayoutId>(descriptors.size());
    map.insert({info, id});
    descriptors.push_back(info);
    vkLayouts.push_back(info.createVkLayout(device));
    return {id, vkLayouts[id]};
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
}