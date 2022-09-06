#pragma once

#include <etna/Vulkan.hpp>
#include <vk_mem_alloc.h>


namespace etna
{

class Buffer
{
public:
  Buffer() = default;

  struct CreateInfo
  {
    std::size_t size;
    vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
  };

  Buffer(VmaAllocator alloc, CreateInfo info);

  Buffer(const Buffer&) = delete;
  Buffer& operator=(const Buffer&) = delete;

  void swap(Buffer& other);
  Buffer(Buffer&&) noexcept;
  Buffer& operator=(Buffer&&) noexcept;

  [[nodiscard]] vk::Buffer get() const { return buffer; }
  [[nodiscard]] std::byte* data() { return mapped; }

  std::byte* map();
  void unmap();

  ~Buffer();

private:
  VmaAllocator allocator{};

  VmaAllocation allocation{};
  vk::Buffer buffer{};
  std::byte* mapped{};
};

}
