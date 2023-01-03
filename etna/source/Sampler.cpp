#include "etna/Sampler.hpp"

#include "etna/GlobalContext.hpp"

#include <etna/DebugUtils.hpp>

namespace etna
{
Sampler::Sampler(CreateInfo info)
{
  vk::SamplerCreateInfo createInfo {
    .magFilter = info.filter,
    .minFilter = info.filter,
    .mipmapMode = vk::SamplerMipmapMode::eLinear,
    .addressModeU = info.addressMode,
    .addressModeV = info.addressMode,
    .addressModeW = info.addressMode,
    .mipLodBias = 0.0f,
    .maxAnisotropy = 1.0f,
    .minLod = 0.0f,
    .maxLod = 1.0f,
    .borderColor = vk::BorderColor::eFloatOpaqueWhite
  };
  sampler = etna::get_context().getDevice().createSamplerUnique(createInfo).value;
  etna::set_debug_name(sampler.get(), info.name.data());
}

}
