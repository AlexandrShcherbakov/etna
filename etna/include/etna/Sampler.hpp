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
    vk::Filter filter = vk::Filter::eNearest;
    vk::SamplerAddressMode addressMode = vk::SamplerAddressMode::eClampToEdge;
    std::string_view name;
    float minLod = 0.0f;
    float maxLod = VK_LOD_CLAMP_NONE;
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
