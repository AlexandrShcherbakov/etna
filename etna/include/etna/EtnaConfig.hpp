#pragma once
#ifndef ETNA_ETNA_CONFIG_HPP_INCLUDED
#define ETNA_ETNA_CONFIG_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{

#if NDEBUG
inline constexpr std::array<const char*, 0> VULKAN_LAYERS = {};
#else
inline constexpr std::array VULKAN_LAYERS = {
  "VK_LAYER_KHRONOS_validation",

#if !defined(__APPLE__)
  "VK_LAYER_LUNARG_monitor"
#endif
};
#endif

inline constexpr uint32_t VULKAN_API_VERSION = vk::ApiVersion13;

inline constexpr const char* ETNA_ENGINE_NAME = "etna";
inline constexpr uint32_t ETNA_VERSION = vk::makeApiVersion(0, ETNA_MAJOR, ETNA_MINOR, ETNA_PATCH);

inline constexpr size_t MAX_FRAMES_INFLIGHT = 4;

} // namespace etna

#endif // ETNA_ETNA_CONFIG_HPP_INCLUDED
