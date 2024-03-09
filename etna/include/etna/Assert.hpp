#pragma once
#ifndef ETNA_ASSERT_HPP_INCLUDED
#define ETNA_ASSERT_HPP_INCLUDED

#include <string_view>
#include <spdlog/spdlog.h>


namespace etna
{
struct SourceLocation
{
  std::string_view file;
  uint32_t line;
};

[[noreturn]] inline void panic(SourceLocation loc, std::string message)
{
  spdlog::critical("Panicked at {}:{}, {}", loc.file, loc.line, message);
  std::terminate();
}
} // namespace etna

#define ETNA_CURRENT_LOCATION                                                                      \
  etna::SourceLocation                                                                             \
  {                                                                                                \
    __FILE__, __LINE__                                                                             \
  }


#define ETNA_PANIC(fmtStr, ...)                                                                    \
  etna::panic(ETNA_CURRENT_LOCATION, fmt::format(fmtStr, ##__VA_ARGS__))

#define ETNA_ASSERTF(expr, fmtStr, ...)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (!static_cast<bool>((expr)))                                                                \
    {                                                                                              \
      ETNA_PANIC("assertion '{}' failed: {}", #expr, fmt::format(fmtStr, ##__VA_ARGS__));          \
    }                                                                                              \
  } while (0)


#define ETNA_ASSERT(expr)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if (!static_cast<bool>((expr)))                                                                \
    {                                                                                              \
      ETNA_PANIC("assertion '{}' failed.", #expr);                                                 \
    }                                                                                              \
  } while (0)

// NOTE: unsanitary macro for customizing vulkan.hpp
// Do NOT use in app code!
#define ETNA_VULKAN_HPP_ASSERT_ON_RESULT(expr) ETNA_ASSERTF(expr, "{}", message)

#endif // ETNA_ASSERT_HPP_INCLUDED
