#include "etna/Sampler.hpp"

#include "etna/GlobalContext.hpp"

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
  sampler = etna::get_context().getDevice().createSampler(createInfo).value;
}

void Sampler::reset()
{
  etna::get_context().getDevice().destroySampler(sampler);
}

Sampler::~Sampler()
{
  reset();
}

void Sampler::swap(Sampler& other)
{
  vk::Sampler tmp = other.sampler;
  other.sampler = sampler;
  sampler = tmp;
}

Sampler::Sampler(Sampler&& other)  noexcept
{
  swap(other);
  other.reset();
}

Sampler& Sampler::operator=(Sampler&& other)  noexcept
{
  swap(other);
  other.reset();
  return *this;
}

}
