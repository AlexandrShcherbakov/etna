#include <etna/Buffer.hpp>


namespace etna
{

Buffer::Buffer(CreateInfo info)
  : allocator{info.allocator}
{
  vk::BufferCreateInfo buf_info{
    .size = info.size,
    .usage = info.bufferUsage,
    .sharingMode = vk::SharingMode::eExclusive
  };

  VmaAllocationCreateInfo alloc_info{
    .usage = info.memoryUsage
  };

  VkBuffer buf;
  auto retcode = vmaCreateBuffer(allocator, &static_cast<const VkBufferCreateInfo&>(buf_info), &alloc_info,
    &buf, &allocation, nullptr);
  if (retcode != VK_SUCCESS)
  {
    throw std::runtime_error("Unable to create a VAM buffer!");
  }
  buffer = buf;
}

void Buffer::swap(Buffer& other)
{
  std::swap(allocator, other.allocator);
  std::swap(allocation, other.allocation);
  std::swap(buffer, other.buffer);
  std::swap(mapped, other.mapped);
}

Buffer::Buffer(Buffer&& other) noexcept
{
  swap(other);
}

Buffer& Buffer::operator=(Buffer&& other) noexcept
{
  if (this == &other)
  {
    return *this;
  }

  if (buffer)
  {
    vmaDestroyBuffer(allocator, buffer, allocation);
    allocator = {};
    allocation = {};
    buffer = vk::Buffer{};
    mapped = {};
  }

  swap(other);

  return *this;
}

Buffer::~Buffer()
{
  if (buffer)
  {
    if (mapped)
    {
      unmap();
    }
    vmaDestroyBuffer(allocator, buffer, allocation);
  }
}

std::byte* Buffer::map()
{
  void* result;
  if (vmaMapMemory(allocator, allocation, &result) != VK_SUCCESS)
  {
    throw std::runtime_error("Mapping failed");
  }

  return mapped = static_cast<std::byte*>(result);
}

void Buffer::unmap()
{
  if (mapped)
  {
    vmaUnmapMemory(allocator, allocation);
    mapped = nullptr;
  }
}

}
