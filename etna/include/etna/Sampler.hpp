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

  Sampler(const Sampler&) = delete;
  Sampler& operator=(const Sampler&) = delete;

  void swap(Sampler& other);
  Sampler(Sampler&&) noexcept;
  Sampler& operator=(Sampler&&) noexcept;

  [[nodiscard]] vk::Sampler get() const { return sampler; }

  ~Sampler();
  void reset();

private:
  vk::Sampler sampler{};
};

}

#endif // ETNA_SAMPLER_HPP_INCLUDED
