#pragma once
#ifndef ETNA_BINDING_ITEMS_HPP_INCLUDED
#define ETNA_BINDING_ITEMS_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{

class Buffer;
class Image;

struct BufferBinding
{
  const Buffer* buffer;
  vk::DescriptorBufferInfo descriptor_info;
};

struct ImageBinding
{
  const Image* image;
  vk::DescriptorImageInfo descriptor_info;
};

struct SamplerBinding
{
  vk::DescriptorImageInfo descriptor_info;
};

} // namespace etna

#endif // ETNA_BINDING_ITEMS_HPP_INCLUDED
