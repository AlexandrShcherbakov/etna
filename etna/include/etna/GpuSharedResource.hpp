#pragma once
#ifndef ETNA_GPU_SHARED_RESOURCE_HPP_INCLUDED
#define ETNA_GPU_SHARED_RESOURCE_HPP_INCLUDED

#include <array>

#include <etna/detail/ManualLifetime.hpp>
#include <etna/EtnaConfig.hpp>
#include <etna/GpuWorkCount.hpp>


namespace etna
{

/**
 * This class can be used to automatically manage multi-buffered GPU-CPU shared
 * resources when in-flight frames are used. Note that this is intentionally
 * designed to always contain valid data, no default constructor is provided.
 * If you want a potentially invalid resource or a late-init resource, use
 * std::optional<GpuSharedResource<T>> and don't forget to check for nullopt.
 * Note that this works well with "pinned", immovable and non-copyable types,
 * and hence is one such type too.
 */
template <class T>
class GpuSharedResource
{
public:
  GpuSharedResource(const GpuSharedResource&) = delete;
  GpuSharedResource& operator=(const GpuSharedResource&) = delete;

  GpuSharedResource(GpuSharedResource&&) = delete;
  GpuSharedResource& operator=(GpuSharedResource&&) = delete;

  template <class... Args>
  explicit GpuSharedResource(const GpuWorkCount& count, std::in_place_t, const Args&... args)
    : workCount{count}
  {
    for (std::size_t i = 0; i < workCount.multiBufferingCount(); ++i)
      impl[i].construct(std::in_place, args...);
  }

  explicit GpuSharedResource(const GpuWorkCount& count, const std::invocable<std::size_t> auto& f)
    : workCount{count}
  {
    for (std::size_t i = 0; i < workCount.multiBufferingCount(); ++i)
      impl[i].construct([&f, i]() { return f(i); });
  }

  T& get() & { return impl[workCount.currentResource()].get(); }
  const T& get() const& { return impl[workCount.currentResource()].get(); }

  template <std::invocable<T&> F>
  void iterate(const F& f) &
  {
    for (std::size_t i = 0; i < workCount.multiBufferingCount(); ++i)
      f(impl[i].get());
  }

  ~GpuSharedResource() noexcept
  {
    for (std::size_t i = 0; i < workCount.multiBufferingCount(); ++i)
      impl[i].destroy();
  }

private:
  const GpuWorkCount& workCount;
  std::array<detail::ManualLifetime<T>, MAX_FRAMES_INFLIGHT> impl;
};

} // namespace etna

#endif // ETNA_GPU_SHARED_RESOURCE_HPP_INCLUDED
