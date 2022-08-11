#include <etna/Context.hpp>
#include <etna/DescriptorSet.hpp>

#include <array>
#include <vector>

namespace etna
{

  bool DescriptorSet::isValid() const
  {
    return get_context().getDescriptorPool().isSetValid(*this);
  }

  /*Todo: Add struct with parameters*/
  static constexpr uint32_t NUM_DESCRIPORS = 2048;

  static constexpr uint32_t NUM_TEXTURES = 2048; 
  static constexpr uint32_t NUM_RW_TEXTURES = 512;
  static constexpr uint32_t NUM_BUFFERS = 2048; 
  static constexpr uint32_t NUM_RW_BUFFERS = 512;
  static constexpr uint32_t NUM_SAMPLERS = 128;

  static constexpr std::array<vk::DescriptorPoolSize, 6> g_default_pool_size {
    vk::DescriptorPoolSize {vk::DescriptorType::eUniformBuffer, NUM_BUFFERS},
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageBuffer, NUM_RW_BUFFERS},
    vk::DescriptorPoolSize {vk::DescriptorType::eSampler, NUM_SAMPLERS},
    vk::DescriptorPoolSize {vk::DescriptorType::eSampledImage, NUM_RW_TEXTURES},
    vk::DescriptorPoolSize {vk::DescriptorType::eStorageImage, NUM_RW_TEXTURES},
    vk::DescriptorPoolSize {vk::DescriptorType::eCombinedImageSampler, NUM_TEXTURES}
  };

  DynamicDescriptorPool::DynamicDescriptorPool(uint32_t frames_in_flight)
    : numFrames {frames_in_flight}
  {
    vk::DescriptorPoolCreateInfo info {};
    info.setMaxSets(NUM_DESCRIPORS);
    info.setPoolSizes(g_default_pool_size);

    for (uint32_t i = 0; i < numFrames; i++)
    {
      pools.push_back(get_context().getDevice().createDescriptorPool(info));
    }
  }

  DynamicDescriptorPool::~DynamicDescriptorPool()
  {
    for (auto pool : pools)
      get_context().getDevice().destroyDescriptorPool(pool);
  }

  void DynamicDescriptorPool::flip()
  {
    frameIndex = (frameIndex + 1) % numFrames;
    flipsCount++;
    get_context().getDevice().resetDescriptorPool(pools[frameIndex]); /*All allocated sets are destroyed*/
  }

  void DynamicDescriptorPool::destroyAllocatedSets()
  {
    for (uint32_t i = 0; i < numFrames; i++)
      flip();
  }
    
  DescriptorSet DynamicDescriptorPool::allocateSet(DescriptorLayoutId layoutId)
  {
    auto vkDevice = get_context().getDevice();
    auto &dslCache = get_context().getDescriptorSetLayouts();
    auto setLayouts = {dslCache.getVkLayout(layoutId)};

    vk::DescriptorSetAllocateInfo info {};
    info.setDescriptorPool(pools[frameIndex]);
    info.setSetLayouts(setLayouts);

    vk::DescriptorSet vkSet {};
    ETNA_ASSERT(vkDevice.allocateDescriptorSets(&info, &vkSet) == vk::Result::eSuccess);
    return DescriptorSet {flipsCount, layoutId, vkSet};
  }

  static bool is_image_resource(vk::DescriptorType dsType)
  {
    switch (dsType)
    {
    case vk::DescriptorType::eUniformBuffer:
    case vk::DescriptorType::eStorageBuffer:
    case vk::DescriptorType::eUniformBufferDynamic:
    case vk::DescriptorType::eStorageBufferDynamic:
      return false;
    case vk::DescriptorType::eCombinedImageSampler:
    case vk::DescriptorType::eSampledImage:
    case vk::DescriptorType::eStorageImage:
    case vk::DescriptorType::eSampler:
      return true;
    default:
      break;
    }

    ETNA_RUNTIME_ERROR("Descriptor write error : unsupported resource ", vk::to_string(dsType));
    return false;
  }

  static void validate_descriptor_write(const DescriptorSet &dst, const vk::ArrayProxy<Binding> &bindings)
  {
    auto &layoutInfo = get_context().getDescriptorSetLayouts().getLayoutInfo(dst.getLayoutId());

    std::array<uint32_t, MAX_DESCRIPTOR_BINDINGS> unboundResources {};

    for (uint32_t binding = 0; binding < MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      unboundResources[binding] = 
        layoutInfo.isBindingUsed(binding)? layoutInfo.getBinding(binding).descriptorCount : 0u;
    }

    for (auto &binding : bindings)
    {
      if (!layoutInfo.isBindingUsed(binding.binding))
        ETNA_RUNTIME_ERROR("Descriptor write error: descriptor set doesn't have ", binding.binding, " slot");

      auto &bindingInfo = layoutInfo.getBinding(binding.binding);
      bool isImageRequied = is_image_resource(bindingInfo.descriptorType); 
      bool isImageBinding = std::get_if<vk::DescriptorImageInfo>(&binding.resources) != nullptr;
      if (isImageRequied != isImageBinding)
      {
        ETNA_RUNTIME_ERROR("Descriptor write error: slot ", binding.binding,
          (isImageRequied? " image required" : " buffer requied"),
          (isImageBinding? " but image bound" : "but buffer bound"));
      }

      unboundResources[binding.binding] -= 1;
    }

    for (uint32_t binding = 0; binding < MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (unboundResources[binding])
        ETNA_RUNTIME_ERROR("Descriptor write error: slot ", binding, " has ", unboundResources[binding], " unbound resources");
    }
  }

  void write_set(const DescriptorSet &dst, const vk::ArrayProxy<Binding> &bindings)
  {
    ETNA_ASSERT(dst.isValid());
    validate_descriptor_write(dst, bindings);

    std::vector<vk::WriteDescriptorSet> writes;
    writes.reserve(bindings.size());

    uint32_t numBufferInfo = 0;
    uint32_t numImageInfo = 0;

    auto &layoutInfo = get_context().getDescriptorSetLayouts().getLayoutInfo(dst.getLayoutId());

    for (auto &binding : bindings)
    {
      auto &bindingInfo = layoutInfo.getBinding(binding.binding);
      if (is_image_resource(bindingInfo.descriptorType))
        numImageInfo++;
      else
        numBufferInfo++;
    }

    std::vector<vk::DescriptorImageInfo> imageInfos;
    std::vector<vk::DescriptorBufferInfo> bufferInfos;
    imageInfos.resize(numImageInfo);
    bufferInfos.resize(numBufferInfo);
    numImageInfo = 0;
    numBufferInfo = 0;

    for (auto &binding : bindings)
    {
      auto &bindingInfo = layoutInfo.getBinding(binding.binding);
      vk::WriteDescriptorSet write {};
      write
        .setDstSet(dst.getVkSet())
        .setDescriptorCount(1)
        .setDstBinding(binding.binding)
        .setDstArrayElement(binding.arrayElem)
        .setDescriptorType(bindingInfo.descriptorType);

      if (is_image_resource(bindingInfo.descriptorType))
      {
        auto img = std::get<vk::DescriptorImageInfo>(binding.resources);
        imageInfos[numImageInfo] = img;
        write.setPImageInfo(imageInfos.data() + numImageInfo);
        numImageInfo++;
      }
      else
      {
        auto buf = std::get<vk::DescriptorBufferInfo>(binding.resources);
        bufferInfos[numBufferInfo] = buf;
        write.setPBufferInfo(bufferInfos.data() + numBufferInfo);
        numBufferInfo++;
      }

      writes.push_back(write);
    }

    get_context().getDevice().updateDescriptorSets(writes, {});
  }
}
