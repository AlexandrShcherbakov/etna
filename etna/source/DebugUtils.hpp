#pragma once
#ifndef DEBUG_UTILS_HPP_INCLUDED
#define DEBUG_UTILS_HPP_INCLUDED

#include <etna/Vulkan.hpp>

namespace etna
{

void set_debug_name(vk::Image image, const char* name);
void set_debug_name(vk::Buffer buffer, const char* name);
void set_debug_name(vk::Sampler sampler, const char* name);

} // namespace etna

#endif // DEBUG_UTILS_HPP_INCLUDED
