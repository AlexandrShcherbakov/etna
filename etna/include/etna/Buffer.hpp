#pragma once
#ifndef ETNA_BUFFER_HPP_INCLUDED
#define ETNA_BUFFER_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <vk_mem_alloc.h>


namespace etna
{

struct BufferBinding;

class Buffer
{
public:
  Buffer() = default;

  struct CreateInfo
  {
    std::size_t size;
    vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    std::string_view name;
  };

  Buffer(VmaAllocator alloc, CreateInfo info);

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  void swap(Buffer& other);
  Buffer(Buffer&&) noexcept;
  Buffer& operator=(Buffer&&) noexcept;

  [[nodiscard]] vk::Buffer get() const { return buffer; }
  [[nodiscard]] std::byte* data() { return mapped; }

  BufferBinding genBinding(vk::DeviceSize offset = 0, vk::DeviceSize range = VK_WHOLE_SIZE) const;

  std::byte* map();
  void unmap();

  ~Buffer();
  void reset();

private:
  VmaAllocator allocator{};

  VmaAllocation allocation{};
  vk::Buffer buffer{};
  std::byte* mapped{};
};

} // namespace etna

#endif // ETNA_BUFFER_HPP_INCLUDED
