#pragma once
#ifndef ETNA_DESCRIPTOR_SET_HPP_INCLUDED
#define ETNA_DESCRIPTOR_SET_HPP_INCLUDED

#include <variant>
#include <vector>

#include <etna/Vulkan.hpp>
#include <etna/GpuWorkCount.hpp>
#include <etna/GpuSharedResource.hpp>
#include <etna/DescriptorSetLayout.hpp>
#include <etna/BindingItems.hpp>
#include <etna/BarrierBehavior.hpp>
#include <etna/GlobalContext.hpp>

namespace etna
{

struct Binding
{
  /*Todo: add resource wrappers*/
  Binding(uint32_t rbinding, const ImageBinding& image_info, uint32_t array_index = 0)
    : binding{rbinding}
    , arrayElem{array_index}
    , resources{image_info}
  {
  }
  Binding(uint32_t rbinding, const BufferBinding& buffer_info, uint32_t array_index = 0)
    : binding{rbinding}
    , arrayElem{array_index}
    , resources{buffer_info}
  {
  }
  Binding(uint32_t rbinding, const SamplerBinding& sampler_info, uint32_t array_index = 0)
    : binding{rbinding}
    , arrayElem{array_index}
    , resources{sampler_info}
  {
  }

  uint32_t binding;
  uint32_t arrayElem;
  std::variant<ImageBinding, BufferBinding, SamplerBinding> resources;
};

/*Maybe we need a hierarchy of descriptor sets*/
struct DescriptorSet
{
  DescriptorSet() {}
  DescriptorSet(
    uint64_t gen,
    DescriptorLayoutId id,
    vk::DescriptorSet vk_set,
    std::vector<Binding> resources,
    vk::CommandBuffer cmd_buffer,
    BarrierBehavior behavior = BarrierBehavior::eDefault)
    : generation{gen}
    , layoutId{id}
    , set{vk_set}
    , bindings{std::move(resources)}
    , command_buffer{cmd_buffer}
  {
    if (get_context().shouldGenerateBarriersWhen(behavior))
    {
      processBarriers();
    }
  }

  bool isValid() const;

  vk::DescriptorSet getVkSet() const { return set; }

  DescriptorLayoutId getLayoutId() const { return layoutId; }

  uint64_t getGen() const { return generation; }

  std::span<Binding const> getBindings() const { return bindings; }

  void processBarriers() const;

private:
  uint64_t generation{};
  DescriptorLayoutId layoutId{};
  vk::DescriptorSet set{};
  std::vector<Binding> bindings{};
  vk::CommandBuffer command_buffer;
};

struct PersistentDescriptorSet
{
  PersistentDescriptorSet() = default;
  PersistentDescriptorSet(
    DescriptorLayoutId id,
    vk::DescriptorSet vk_set,
    std::vector<Binding> resources,
    bool allow_unbound_slots)
    : layoutId{id}
    , set{vk_set}
    , bindings{std::move(resources)}
    , allowUnboundSlots{allow_unbound_slots}
  {
  }

  bool isValid() const { return set != vk::DescriptorSet{}; }

  vk::DescriptorSet getVkSet() const { return set; }

  DescriptorLayoutId getLayoutId() const { return layoutId; }

  std::span<Binding const> getBindings() const { return bindings; }

  void processBarriers(vk::CommandBuffer cmd_buffer) const;

  // @NOTE: has to be called BEFORE binding the dset
  void updateBindings(std::span<Binding const> new_bindings);

private:
  DescriptorLayoutId layoutId{};
  vk::DescriptorSet set{};
  std::vector<Binding> bindings{};
  bool allowUnboundSlots = false;
};

/**
 * Base version. Allocate and use descriptor sets while writing command buffer, they will be
 * destroyed automaticaly. Resource allocation tracking shoud be added. For long-living descriptor
 * sets (e.g bindless resource sets) separate allocator shoud be added, with ManagedDescriptorSet
 * with destructor
 */
struct DynamicDescriptorPool
{
  DynamicDescriptorPool(vk::Device dev, const GpuWorkCount& work_count);

  void beginFrame();
  void destroyAllocatedSets();
  void reset(uint32_t frames_in_flight);

  DescriptorSet allocateSet(
    DescriptorLayoutId layout_id,
    std::vector<Binding> bindings,
    vk::CommandBuffer command_buffer,
    BarrierBehavior behavior = BarrierBehavior::eDefault);

  bool isSetValid(const DescriptorSet& set) const
  {
    return set.getVkSet() &&
      set.getGen() + workCount.multiBufferingCount() > workCount.batchIndex();
  }

private:
  vk::Device vkDevice;
  const GpuWorkCount& workCount;

  GpuSharedResource<vk::UniqueDescriptorPool> pools;
};

/**
 * Base version. Allocate persistent descriptor sets from here, which will
 * never be destroyed until program terminates. As stated above, this could
 * be replaced with resource tracking.
 */
struct PersistentDescriptorPool
{
  explicit PersistentDescriptorPool(vk::Device dev);

  PersistentDescriptorSet allocateSet(
    DescriptorLayoutId layout_id, std::vector<Binding> bindings, bool allow_unbound_slots = false);

private:
  vk::Device vkDevice;
  vk::UniqueDescriptorPool pool;
};

template <class TDescriptorSet>
void write_set(
  const TDescriptorSet& dst, std::span<Binding const> bindings, bool allow_unbound_slots = false);

uint32_t get_num_descriptors_in_pool_for_type(vk::DescriptorType type);

} // namespace etna

#endif // ETNA_DESCRIPTOR_SET_HPP_INCLUDED
