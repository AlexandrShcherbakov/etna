#pragma once
#ifndef ETNA_SAMPLER_HPP_INCLUDED
#define ETNA_SAMPLER_HPP_INCLUDED

#include <etna/Etna.hpp>


namespace etna
{

class Sampler
{
public:
  Sampler() = default;

  struct CreateInfo
  {
    vk::Filter filter = vk::Filter::eLinear;
    vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eClampToEdge;
    vk::SamplerMipmapMode mipmapMode = vk::SamplerMipmapMode::eLinear;
    std::string_view name;
    float minLod = 0.0f;
    float maxLod = VK_LOD_CLAMP_NONE;
    bool compareEnable = false;
    vk::CompareOp compareOp = vk::CompareOp::eLessOrEqual;
  };

  explicit Sampler(CreateInfo info);

  [[nodiscard]] vk::Sampler get() const { return sampler.get(); }

  // Creates a binding to be used with etna::Binding and etna::create_descriptor_set
  SamplerBinding genBinding() const;

private:
  vk::UniqueSampler sampler{};
};

} // namespace etna

#endif // ETNA_SAMPLER_HPP_INCLUDED
