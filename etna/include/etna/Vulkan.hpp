#pragma once
#ifndef ETNA_VULKAN_HPP
#define ETNA_VULKAN_HPP


// This wrapper has to be used so that our
// macro customizations work
#include <etna/Assert.hpp>


#ifndef NDEBUG
#define VULKAN_HPP_ASSERT ETNA_VERIFY
#define VULKAN_HPP_ASSERT_ON_RESULT(expr) ETNA_VERIFYF(expr, "{}", message)
#else
#define VULKAN_HPP_ASSERT(expr)
#define VULKAN_HPP_ASSERT_ON_RESULT(expr)
#endif

// vulkan.hpp includes windows.h :(
#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#endif

// Need to enable beta extensions for VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME to be declared
#if defined(__APPLE__)
#define VK_ENABLE_BETA_EXTENSIONS
#endif

#include <vulkan/vulkan.hpp>

#define ETNA_CHECK_VK_RESULT(expr)                                                                 \
  do                                                                                               \
  {                                                                                                \
    auto _resfd1fcd5e = expr;                                                                      \
    ETNA_VERIFYF(                                                                                  \
      _resfd1fcd5e == vk::Result::eSuccess, "Vulkan error: {}", vk::to_string(_resfd1fcd5e));      \
  } while (0)

namespace etna
{

template <typename T>
T unwrap_vk_result(vk::ResultValue<T>&& result_val)
{
  ETNA_VERIFYF(
    result_val.result == vk::Result::eSuccess,
    "Vulkan error: {}",
    vk::to_string(result_val.result));
  return std::move(result_val.value);
}

} // namespace etna

#endif // ETNA_VULKAN_HPP
