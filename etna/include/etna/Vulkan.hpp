#pragma once
#ifndef ETNA_VULKAN_HPP
#define ETNA_VULKAN_HPP

// This wrapper has to be used so that our
// macro customizations work
#include <etna/Assert.hpp>
#define VULKAN_HPP_ASSERT ETNA_ASSERT
#define VULKAN_HPP_ASSERT_ON_RESULT ETNA_VULKAN_HPP_ASSERT_ON_RESULT
// vulkan.hpp somehow includes windows.h :(
#if defined(_WIN32) || defined(_WIN64)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#endif
#include <vulkan/vulkan.hpp>

#endif // ETNA_VULKAN_HPP
