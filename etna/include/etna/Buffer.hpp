#pragma once
#ifndef ETNA_BUFFER_HPP_INCLUDED
#define ETNA_BUFFER_HPP_INCLUDED

#include <string_view>

#include <etna/Vulkan.hpp>
#include <etna/BindingItems.hpp>
#include <vk_mem_alloc.h>


namespace etna
{

class Buffer
{
public:
  Buffer() = default;

  // Settings for creating a new buffer
  struct CreateInfo
  {
    // Size of the buffer in bytes
    vk::DeviceSize size;

    // How will this buffer be used?
    vk::BufferUsageFlags bufferUsage = vk::BufferUsageFlagBits::eStorageBuffer;

    // Basically determines the memory type this buffer will live in.
    // You want AUTO_PREFER_DEVICE for stuff that is produced on the GPU or read very often
    // on the GPU and not updated from the CPU all that often (and update it via copies).
    // Otherwise, feel free to use VMA_MEMORY_USAGE_AUTO.
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // Basically determines the memory properties for the buffer.
    // You want HOST_ACCESS_SEQUENTIAL_WRITE_BIT | CREATE_MAPPED_BIT for uniform buffers,
    // that you want to map and write from the CPU and read on GPU
    // See recommended usage patterns:
    // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
    VmaAllocationCreateFlags allocationCreate = 0;

    // Name of the image for debugging tools
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

  BufferBinding genBinding(vk::DeviceSize offset = 0, vk::DeviceSize range = vk::WholeSize) const;

  // If the buffer is in CPU_TO_GPU or GPU_TO_CPU memory, returns a CPU-accessible
  // pointer to the start of this buffer's bytes, which can be used for reading or
  // writing the buffer (preferably in a linear manner).
  std::byte* map();

  // Invalidates the pointer returned by map.
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
