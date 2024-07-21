#pragma once
#ifndef ETNA_DETAIL_MANUAL_LIFETIME_INCLUDED
#define ETNA_DETAIL_MANUAL_LIFETIME_INCLUDED

#include <memory>


namespace etna::detail
{

template <class T>
class ManualLifetime
{
public:
  constexpr ManualLifetime() noexcept {}

  constexpr ~ManualLifetime() {}

  ManualLifetime(const ManualLifetime&) = delete;
  ManualLifetime& operator=(const ManualLifetime&) = delete;

  ManualLifetime(ManualLifetime&&) = delete;
  ManualLifetime& operator=(ManualLifetime&&) = delete;

  template <class... Args>
  T& construct(std::in_place_t, Args&&... args) noexcept(
    std::is_nothrow_constructible_v<T, Args...>)
  {
    return *std::construct_at(&get(), std::forward<Args>(args)...);
  }

  template <std::invocable<> F>
  T& construct(F&& func)
  {
    return *::new (static_cast<void*>(storage)) T(std::forward<F&&>(func)());
  }

  void destroy() noexcept { std::destroy_at(&get()); }

  T& get() & noexcept { return *reinterpret_cast<T*>(storage); }
  const T& get() const& noexcept { return *reinterpret_cast<const T*>(storage); }

private:
  alignas(T) std::byte storage[sizeof(T)]{};
};

} // namespace etna::detail

#endif // ETNA_DETAIL_MANUAL_LIFETIME_INCLUDED
