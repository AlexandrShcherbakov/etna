#pragma once
#ifndef ETNA_UTIL_HPP_INCLUDED
#define ETNA_UTIL_HPP_INCLUDED

#include <stdexcept>
#include <sstream>
#include <cassert>
namespace etna
{
  namespace internal
  {    
    template <typename T, typename... Args>  
    void collect_args(std::stringstream &stream, T &&val, Args&&... args)
    {
      stream << val;
      if constexpr (sizeof...(Args))
      {
        collect_args(stream, std::forward<Args>(args)...);
      }
    }
  
  }
  
  template <typename... Args>
  void runtime_error(Args&&... args)
  {
    std::stringstream stream;
    internal::collect_args(stream, std::forward<Args>(args)...);
    throw std::runtime_error {stream.str()};
  }
}

#define ETNA_RUNTIME_ERROR(...) etna::runtime_error("(", __FILE__, ":", __LINE__, ")", __VA_ARGS__)
#define ETNA_ASSERT(expr) (static_cast<bool>(expr)? void(0) : ETNA_RUNTIME_ERROR("Assert", # expr, " failed"))
#define ETNA_VK_ASSERT(expr) ETNA_ASSERT((expr) == vk::Result::eSuccess)

#endif
