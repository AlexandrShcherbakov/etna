#include <etna/GlobalContext.hpp>

#include <array>
#include <vector>

#include <etna/DescriptorSet.hpp>
#include <etna/Etna.hpp>


namespace etna
{

bool DescriptorSet::isValid() const
{
  return get_context().getDescriptorPool().isSetValid(*this);
}

/*Todo: Add struct with parameters*/
static constexpr uint32_t NUM_DESCRIPTORS = 2048;

static constexpr uint32_t NUM_TEXTURES = 2048;
static constexpr uint32_t NUM_RW_TEXTURES = 512;
static constexpr uint32_t NUM_BUFFERS = 2048;
static constexpr uint32_t NUM_RW_BUFFERS = 512;
static constexpr uint32_t NUM_SAMPLERS = 128;

static constexpr std::array<vk::DescriptorPoolSize, 6> DEFAULT_POOL_SIZES{
  vk::DescriptorPoolSize{vk::DescriptorType::eUniformBuffer, NUM_BUFFERS},
  vk::DescriptorPoolSize{vk::DescriptorType::eStorageBuffer, NUM_RW_BUFFERS},
  vk::DescriptorPoolSize{vk::DescriptorType::eSampler, NUM_SAMPLERS},
  vk::DescriptorPoolSize{vk::DescriptorType::eSampledImage, NUM_RW_TEXTURES},
  vk::DescriptorPoolSize{vk::DescriptorType::eStorageImage, NUM_RW_TEXTURES},
  vk::DescriptorPoolSize{vk::DescriptorType::eCombinedImageSampler, NUM_TEXTURES}};

DynamicDescriptorPool::DynamicDescriptorPool(vk::Device dev, const GpuWorkCount& work_count)
  : vkDevice{dev}
  , workCount{work_count}
  , pools{work_count, [dev](std::size_t) {
            vk::DescriptorPoolCreateInfo info{
              .maxSets = NUM_DESCRIPTORS,
              .poolSizeCount = static_cast<std::uint32_t>(DEFAULT_POOL_SIZES.size()),
              .pPoolSizes = DEFAULT_POOL_SIZES.data()};
            return unwrap_vk_result(dev.createDescriptorPoolUnique(info));
          }}
{
}

void DynamicDescriptorPool::beginFrame()
{
  vkDevice.resetDescriptorPool(pools.get().get());
}

void DynamicDescriptorPool::destroyAllocatedSets()
{
  pools.iterate([this](auto& pool) { vkDevice.resetDescriptorPool(pool.get()); });
}

DescriptorSet DynamicDescriptorPool::allocateSet(
  DescriptorLayoutId layout_id,
  std::vector<Binding> bindings,
  vk::CommandBuffer command_buffer,
  BarrierBehavior behavior)
{
  auto& dslCache = get_context().getDescriptorSetLayouts();
  auto setLayouts = {dslCache.getVkLayout(layout_id)};

  vk::DescriptorSetAllocateInfo info{};
  info.setDescriptorPool(pools.get().get());
  info.setSetLayouts(setLayouts);

  vk::DescriptorSet vkSet{};
  ETNA_VERIFY(vkDevice.allocateDescriptorSets(&info, &vkSet) == vk::Result::eSuccess);
  return DescriptorSet{
    workCount.batchIndex(), layout_id, vkSet, std::move(bindings), command_buffer, behavior};
}

PersistentDescriptorPool::PersistentDescriptorPool(vk::Device dev)
  : vkDevice{dev}
  , pool{unwrap_vk_result(dev.createDescriptorPoolUnique(vk::DescriptorPoolCreateInfo{
      .maxSets = NUM_DESCRIPTORS,
      .poolSizeCount = static_cast<std::uint32_t>(DEFAULT_POOL_SIZES.size()),
      .pPoolSizes = DEFAULT_POOL_SIZES.data()}))}
{
}

PersistentDescriptorSet PersistentDescriptorPool::allocateSet(
  DescriptorLayoutId layout_id, std::vector<Binding> bindings)
{
  auto& dslCache = get_context().getDescriptorSetLayouts();
  auto setLayouts = {dslCache.getVkLayout(layout_id)};

  vk::DescriptorSetAllocateInfo info{};
  info.setDescriptorPool(pool.get());
  info.setSetLayouts(setLayouts);

  vk::DescriptorSet vkSet{};
  ETNA_VERIFY(vkDevice.allocateDescriptorSets(&info, &vkSet) == vk::Result::eSuccess);
  return PersistentDescriptorSet{layout_id, vkSet, std::move(bindings)};
}

static bool is_image_resource(vk::DescriptorType ds_type)
{
  switch (ds_type)
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

  ETNA_PANIC("Descriptor write error : unsupported resource {}", vk::to_string(ds_type));
}

template <class TDescriptorSet>
static void validate_descriptor_write(const TDescriptorSet& dst, bool allow_unbound_slots)
{
  const auto& layoutInfo = get_context().getDescriptorSetLayouts().getLayoutInfo(dst.getLayoutId());
  const auto& bindings = dst.getBindings();

  std::array<uint32_t, MAX_DESCRIPTOR_BINDINGS> unboundResources{};

  for (uint32_t binding = 0; binding < MAX_DESCRIPTOR_BINDINGS; binding++)
  {
    unboundResources[binding] =
      layoutInfo.isBindingUsed(binding) ? layoutInfo.getBinding(binding).descriptorCount : 0u;
  }

  for (const auto& binding : bindings)
  {
    if (!layoutInfo.isBindingUsed(binding.binding))
      ETNA_PANIC("Descriptor write error: descriptor set doesn't have {} slot", binding.binding);

    const auto& bindingInfo = layoutInfo.getBinding(binding.binding);
    bool isImageRequired = is_image_resource(bindingInfo.descriptorType);
    bool isImageBinding = std::get_if<ImageBinding>(&binding.resources) != nullptr;
    bool isSamplerBinding = std::get_if<SamplerBinding>(&binding.resources) != nullptr;
    if (isImageRequired != (isImageBinding || isSamplerBinding))
    {
      ETNA_PANIC(
        "Descriptor write error: slot {} {} required but {} bound",
        binding.binding,
        (isImageRequired ? "image/sampler" : "buffer"),
        (isImageBinding ? "image" : (isSamplerBinding ? "sampler" : "buffer")));
    }

    unboundResources[binding.binding] -= 1;
  }

  if (!allow_unbound_slots)
  {
    for (uint32_t binding = 0; binding < MAX_DESCRIPTOR_BINDINGS; binding++)
    {
      if (unboundResources[binding] > 0)
        ETNA_PANIC(
          "Descriptor write error: slot {} has {} unbound resources",
          binding,
          unboundResources[binding]);
    }
  }
}

template <class TDescriptorSet>
void write_set(const TDescriptorSet& dst, bool allow_unbound_slots)
{
  ETNA_VERIFY(dst.isValid());
  validate_descriptor_write(dst, allow_unbound_slots);

  std::vector<vk::WriteDescriptorSet> writes;
  writes.reserve(dst.getBindings().size());

  uint32_t numBufferInfo = 0;
  uint32_t numImageInfo = 0;

  const auto& layoutInfo = get_context().getDescriptorSetLayouts().getLayoutInfo(dst.getLayoutId());

  for (auto& binding : dst.getBindings())
  {
    const auto& bindingInfo = layoutInfo.getBinding(binding.binding);
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

  for (const auto& binding : dst.getBindings())
  {
    const auto& bindingInfo = layoutInfo.getBinding(binding.binding);
    vk::WriteDescriptorSet write{};
    write.setDstSet(dst.getVkSet())
      .setDescriptorCount(1)
      .setDstBinding(binding.binding)
      .setDstArrayElement(binding.arrayElem)
      .setDescriptorType(bindingInfo.descriptorType);

    if (is_image_resource(bindingInfo.descriptorType))
    {
      const auto* imgMaybe = std::get_if<ImageBinding>(&binding.resources);
      const auto* smpMaybe = std::get_if<SamplerBinding>(&binding.resources);
      const auto& descriptorInfo =
        imgMaybe != nullptr ? imgMaybe->descriptor_info : smpMaybe->descriptor_info;
      imageInfos[numImageInfo] = descriptorInfo;
      write.setPImageInfo(imageInfos.data() + numImageInfo);
      numImageInfo++;
    }
    else
    {
      const auto buf = std::get<BufferBinding>(binding.resources).descriptor_info;
      bufferInfos[numBufferInfo] = buf;
      write.setPBufferInfo(bufferInfos.data() + numBufferInfo);
      numBufferInfo++;
    }

    writes.push_back(write);
  }

  get_context().getDevice().updateDescriptorSets(writes, {});
}

template void write_set<DescriptorSet>(const DescriptorSet&, bool);
template void write_set<PersistentDescriptorSet>(const PersistentDescriptorSet&, bool);

constexpr static vk::PipelineStageFlags2 shader_stage_to_pipeline_stage(
  vk::ShaderStageFlags shader_stages)
{
  constexpr uint32_t MAPPING_LENGTH = 6;
  constexpr std::array<vk::ShaderStageFlagBits, MAPPING_LENGTH> SHADER_STAGES = {
    vk::ShaderStageFlagBits::eVertex,
    vk::ShaderStageFlagBits::eTessellationControl,
    vk::ShaderStageFlagBits::eTessellationEvaluation,
    vk::ShaderStageFlagBits::eGeometry,
    vk::ShaderStageFlagBits::eFragment,
    vk::ShaderStageFlagBits::eCompute,
  };
  constexpr std::array<vk::PipelineStageFlagBits2, MAPPING_LENGTH> PIPELINE_STAGES = {
    vk::PipelineStageFlagBits2::eVertexShader,
    vk::PipelineStageFlagBits2::eTessellationControlShader,
    vk::PipelineStageFlagBits2::eTessellationEvaluationShader,
    vk::PipelineStageFlagBits2::eGeometryShader,
    vk::PipelineStageFlagBits2::eFragmentShader,
    vk::PipelineStageFlagBits2::eComputeShader,
  };

  vk::PipelineStageFlags2 pipelineStages = vk::PipelineStageFlagBits2::eNone;
  for (uint32_t i = 0; i < MAPPING_LENGTH; ++i)
  {
    if (SHADER_STAGES[i] & shader_stages)
      pipelineStages |= PIPELINE_STAGES[i];
  }

  return pipelineStages;
}

constexpr static vk::AccessFlags2 descriptor_type_to_access_flag(vk::DescriptorType descriptor_type)
{
  constexpr uint32_t MAPPING_LENGTH = 3;
  constexpr std::array<vk::DescriptorType, MAPPING_LENGTH> DESCRIPTOR_TYPES = {
    vk::DescriptorType::eSampledImage,
    vk::DescriptorType::eStorageImage,
    vk::DescriptorType::eCombinedImageSampler,
  };
  constexpr std::array<vk::AccessFlags2, MAPPING_LENGTH> ACCESS_FLAGS = {
    vk::AccessFlagBits2::eShaderSampledRead,
    vk::AccessFlagBits2::eShaderStorageRead | vk::AccessFlagBits2::eShaderStorageWrite,
    vk::AccessFlagBits2::eShaderSampledRead,
  };
  for (uint32_t i = 0; i < MAPPING_LENGTH; ++i)
  {
    if (DESCRIPTOR_TYPES[i] == descriptor_type)
      return ACCESS_FLAGS[i];
  }
  return vk::AccessFlagBits2::eNone;
}

static void process_barriers_to_cmd_buf(
  vk::CommandBuffer cmd_buffer,
  DescriptorLayoutId layout_id,
  std::span<const etna::Binding> bindings)
{
  auto& layoutInfo = get_context().getDescriptorSetLayouts().getLayoutInfo(layout_id);
  for (auto& binding : bindings)
  {
    if (std::get_if<ImageBinding>(&binding.resources) == nullptr)
      continue; // Add processing for buffer here if you need.

    auto& bindingInfo = layoutInfo.getBinding(binding.binding);
    const ImageBinding& imgData = std::get<ImageBinding>(binding.resources);
    etna::set_state(
      cmd_buffer,
      imgData.image.get(),
      shader_stage_to_pipeline_stage(bindingInfo.stageFlags),
      descriptor_type_to_access_flag(bindingInfo.descriptorType),
      imgData.descriptor_info.imageLayout,
      imgData.image.getAspectMaskByFormat());
  }
}

void DescriptorSet::processBarriers() const
{
  process_barriers_to_cmd_buf(command_buffer, layoutId, bindings);
}

void PersistentDescriptorSet::processBarriers(vk::CommandBuffer cmd_buffer) const
{
  process_barriers_to_cmd_buf(cmd_buffer, layoutId, bindings);
}

} // namespace etna
