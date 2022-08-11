#pragma once
#ifndef ETNA_UPDATE_DESCRIPTOR_SETS_HPP_INCLUDED
#define ETNA_UPDATE_DESCRIPTOR_SETS_HPP_INCLUDED

#include <variant>
#include <vulkan/vulkan.hpp>
#include "DescriptorSetLayout.hpp"

namespace etna
{
  /*Maybe we need a hierarchy of descriptor sets*/
  struct DescriptorSet
  {
    DescriptorSet() {}
    DescriptorSet(uint64_t gen, DescriptorLayoutId id, vk::DescriptorSet vk_set)
      : generation {gen}, layoutId {id}, set {vk_set} {}

    bool isValid() const;
    
    vk::DescriptorSet getVkSet() const
    {
      return set;
    }

    DescriptorLayoutId getLayoutId() const
    {
      return layoutId;
    }

    uint64_t getGen() const
    {
      return generation;
    }

  private:
    uint64_t generation {};
    DescriptorLayoutId layoutId {};
    vk::DescriptorSet set{};
  };

  /*Base version. Allocate and use descriptor sets while writing command buffer, they will be destroyed
  automaticaly. Resource allocation tracking shoud be added. 
  For long-living descriptor sets (e.g bindless resource sets) separate allocator shoud be added, with ManagedDescriptorSet with destructor*/
  struct DynamicDescriptorPool
  {
    DynamicDescriptorPool() {}
    ~DynamicDescriptorPool();
    
    void flip();
    void destroyAllocatedSets();
    void reset(uint32_t frames_in_flight);

    DescriptorSet allocateSet(DescriptorLayoutId layoutId);

    vk::DescriptorPool getCurrentPool() const
    {
      return pools[frameIndex];
    }
    
    uint64_t getNumFlips() const
    {
      return flipsCount;
    }

    bool isSetValid(const DescriptorSet &set) const
    {
      return set.getVkSet() && set.getGen() + numFrames > flipsCount;
    }

  private:
    uint32_t numFrames = 0;
    uint32_t frameIndex = 0;
    uint64_t flipsCount = 0; /*for tracking invalid sets*/
    std::vector<vk::DescriptorPool> pools;
  };

  struct Binding
  {
    /*Todo: add resource wrappers*/
    Binding(uint32_t rbinding, const vk::DescriptorImageInfo &image_info, uint32_t array_index = 0)
      : binding {rbinding}, arrayElem {array_index}, resources {image_info} {}
    Binding(uint32_t rbinding, const vk::DescriptorBufferInfo &buffer_info, uint32_t array_index = 0)
      : binding {rbinding}, arrayElem {array_index}, resources {buffer_info} {}
  
    uint32_t binding;
    uint32_t arrayElem;
    std::variant<vk::DescriptorImageInfo, vk::DescriptorBufferInfo> resources;
  };

  void write_set(const DescriptorSet &dst, const vk::ArrayProxy<Binding> &bindings);
}

#endif
