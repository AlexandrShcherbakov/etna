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
  };

  Sampler(CreateInfo info);

  [[nodiscard]] vk::Sampler get() const { return sampler.get(); }

private:
  vk::UniqueSampler sampler{};
};

}

#endif // ETNA_SAMPLER_HPP_INCLUDED
