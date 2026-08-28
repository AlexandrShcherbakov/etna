#include <etna/Sampler.hpp>

#include <algorithm>

#include <etna/GlobalContext.hpp>
#include "DebugUtils.hpp"

namespace etna
{

Sampler::Sampler(CreateInfo info)
{
  const auto physicalDeviceProperties = etna::get_context().getPhysicalDevice().getProperties();
  const float maxAnisotropy =
    std::min(info.maxAnisotropy, physicalDeviceProperties.limits.maxSamplerAnisotropy);
  vk::SamplerCreateInfo createInfo{
    .magFilter = info.filter,
    .minFilter = info.filter,
    .mipmapMode = info.mipmapMode,
    .addressModeU = info.addressMode,
    .addressModeV = info.addressMode,
    .addressModeW = info.addressMode,
    .mipLodBias = 0.0f,
    .anisotropyEnable = static_cast<vk::Bool32>(info.enableAnisotropy),
    .maxAnisotropy = maxAnisotropy,
    .compareEnable = static_cast<vk::Bool32>(info.compareEnable),
    .compareOp = info.compareOp,
    .minLod = info.minLod,
    .maxLod = info.maxLod,
    .borderColor = info.borderColor,
  };
  sampler = unwrap_vk_result(etna::get_context().getDevice().createSamplerUnique(createInfo));
  etna::set_debug_name(sampler.get(), info.name.data());
}

SamplerBinding Sampler::genBinding() const
{
  return SamplerBinding{vk::DescriptorImageInfo{sampler.get()}};
}

} // namespace etna
