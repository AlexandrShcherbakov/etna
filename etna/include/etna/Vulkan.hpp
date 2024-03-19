#pragma once
#ifndef ETNA_VULKAN_HPP
#define ETNA_VULKAN_HPP


// This wrapper has to be used so that our
// macro customizations work
#include <etna/Assert.hpp>

// Note: vulkan.hpp expects assertion macros arguments to not
// be evaluated with NDEBUG but etna's assertion macros do
// evaluate them and discard results.
#if !defined(NDEBUG)
#define VULKAN_HPP_ASSERT ETNA_ASSERT
#define VULKAN_HPP_ASSERT_ON_RESULT ETNA_VULKAN_HPP_ASSERT_ON_RESULT
#else
#define VULKAN_HPP_ASSERT(EXPR)
#define VULKAN_HPP_ASSERT_ON_RESULT(EXPR)
#endif

// vulkan.hpp includes windows.h :(
#if defined(_WIN32) || defined(_WIN64)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#endif
#include <vulkan/vulkan.hpp>

#define ETNA_CHECK_VK_RESULT(expr) ETNA_ASSERT((expr) == vk::Result::eSuccess)

namespace etna
{

template <typename T>
T unwrap_vk_result(vk::ResultValue<T>&& resultVal)
{
  ETNA_CHECK_VK_RESULT(resultVal.result);
  return resultVal.value;
}

}

#endif // ETNA_VULKAN_HPP
