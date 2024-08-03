#pragma once
#ifndef ETNA_VULKAN_FORMATTER_HPP_INCLUDED
#define ETNA_VULKAN_FORMATTER_HPP_INCLUDED

#include <fmt/format.h>

#include <etna/Vulkan.hpp>


// Format anything formattable via vk::to_string as such
template <class T>
  requires requires(T t) { vk::to_string(t); }
struct fmt::formatter<T> : formatter<std::string>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
  template <typename Context>
  constexpr auto format(T t, Context& ctx) const
  {
    return formatter<std::string>::format(vk::to_string(t), ctx);
  }
};

#endif // ETNA_VULKAN_FORMATTER_HPP_INCLUDED
