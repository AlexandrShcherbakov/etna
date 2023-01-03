#pragma once
#ifndef ETNA_STRING_MAP_HPP_INCLUDED
#define ETNA_STRING_MAP_HPP_INCLUDED


#include <string>
#include <string_view>
#include <unordered_map>


namespace etna
{

namespace detail
{

// Transparent policies so that everything works with string_views
// Copied from a MSVC STL github issue

struct string_compare
{
  using is_transparent = void;

  bool operator()(std::string_view key, std::string_view txt) const { return key == txt; }
  bool operator()(std::string key, std::string_view txt) const { return key == txt; }
};

struct string_hash
{
  using is_transparent = void;
  using transparent_key_equal = string_compare;

  size_t operator()(std::string_view txt) const { return std::hash<std::string_view>{}(txt); }
  size_t operator()(const std::string& txt) const { return std::hash<std::string>{}(txt); }
};

}

template<class Value, class Allocator = std::allocator<std::pair<const std::string, Value>>>
using StringMap = std::unordered_map<std::string, Value, detail::string_hash, detail::string_compare, Allocator>;

}

#endif // ETNA_STRING_MAP_HPP_INCLUDED
