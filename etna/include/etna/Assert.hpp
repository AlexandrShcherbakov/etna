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
  etna::panic(ETNA_CURRENT_LOCATION, fmt::format(fmtStr __VA_OPT__(, ) __VA_ARGS__))


// VERIFY macros should be used for checks that must happen even in release builds.
#define ETNA_VERIFYF(expr, fmtStr, ...)                                                            \
  do                                                                                               \
  {                                                                                                \
    if (!static_cast<bool>((expr)))                                                                \
    {                                                                                              \
      ETNA_PANIC(                                                                                  \
        "assertion '{}' failed: {}", #expr, fmt::format(fmtStr __VA_OPT__(, ) __VA_ARGS__));       \
    }                                                                                              \
  } while (0)


// VERIFY macros should be used for checks that must happen even in release builds.
#define ETNA_VERIFY(expr)                                                                          \
  do                                                                                               \
  {                                                                                                \
    if (!static_cast<bool>((expr)))                                                                \
    {                                                                                              \
      ETNA_PANIC("assertion '{}' failed.", #expr);                                                 \
    }                                                                                              \
  } while (0)

#if ETNA_DEBUG
// ASSERT macros are cut out of the release binary and should be used to check **invariants**.
#define ETNA_ASSERT(expr) ETNA_VERIFY(expr)
// ASSERT macros are cut out of the release binary and should be used to check **invariants**.
#define ETNA_ASSERTF(expr, fmtStr, ...) ETNA_VERIFYF(expr, fmtStr __VA_OPT__(, ) __VA_ARGS__)
#else
#define ETNA_ASSERT(expr)
#define ETNA_ASSERTF(expr, fmtStr...)
#endif

#endif // ETNA_ASSERT_HPP_INCLUDED
