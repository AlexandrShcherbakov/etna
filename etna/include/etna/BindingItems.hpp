#pragma once
#ifndef ETNA_BINDING_ITENS_HPP_INCLUDED
#define ETNA_BINDING_ITENS_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{
  class Buffer;
  class Image;

  struct BufferBinding
  {
    const Buffer &buffer;
    vk::DescriptorBufferInfo descriptor_info;
  };

  struct ImageBinding
  {
    const Image &image;
    vk::DescriptorImageInfo descriptor_info;
  };
}

#endif // ETNA_BINDING_ITENS_HPP_INCLUDED
