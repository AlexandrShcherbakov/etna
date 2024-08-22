#include <etna/Sampler.hpp>

#include <etna/GlobalContext.hpp>
#include "DebugUtils.hpp"

namespace etna
{

Sampler::Sampler(CreateInfo info)
{
  vk::SamplerCreateInfo createInfo{
    .magFilter = info.filter,
    .minFilter = info.filter,
    .mipmapMode = vk::SamplerMipmapMode::eLinear,
    .addressModeU = info.addressMode,
    .addressModeV = info.addressMode,
    .addressModeW = info.addressMode,
    .mipLodBias = 0.0f,
    .maxAnisotropy = 1.0f,
    .minLod = info.minLod,
    .maxLod = info.maxLod,
    .borderColor = vk::BorderColor::eFloatOpaqueWhite,
  };
  sampler = unwrap_vk_result(etna::get_context().getDevice().createSamplerUnique(createInfo));
  etna::set_debug_name(sampler.get(), info.name.data());
}

} // namespace etna
