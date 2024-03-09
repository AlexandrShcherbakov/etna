#include <etna/Buffer.hpp>

#include <etna/BindingItems.hpp>
#include "DebugUtils.hpp"


namespace etna
{

Buffer::Buffer(VmaAllocator alloc, CreateInfo info)
    : allocator{alloc}
{
  vk::BufferCreateInfo buf_info{
    .size = info.size, .usage = info.bufferUsage, .sharingMode = vk::SharingMode::eExclusive};

  VmaAllocationCreateInfo alloc_info{
    .flags = 0,
    .usage = info.memoryUsage,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = nullptr,
    .pUserData = nullptr,
    .priority = 0.f};

  VkBuffer buf;
  auto retcode = vmaCreateBuffer(
    allocator,
    &static_cast<const VkBufferCreateInfo&>(buf_info),
    &alloc_info,
    &buf,
    &allocation,
    nullptr);
  // Note that usually vulkan.hpp handles doing the assertion
  // and a pretty message, but VMA cannot do that.
  ETNA_ASSERTF(
    retcode == VK_SUCCESS,
    "Error {} occurred while trying to allocate an etna::Buffer!",
    vk::to_string(static_cast<vk::Result>(retcode)));
  buffer = vk::Buffer(buf);
  etna::set_debug_name(buffer, info.name.data());
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
    return *this;

  reset();
  swap(other);

  return *this;
}

Buffer::~Buffer()
{
  reset();
}

void Buffer::reset()
{
  if (!buffer)
    return;

  if (mapped != nullptr)
    unmap();

  vmaDestroyBuffer(allocator, VkBuffer(buffer), allocation);
  allocator = {};
  allocation = {};
  buffer = vk::Buffer{};
}

std::byte* Buffer::map()
{
  void* result;

  // I can't think of a use case where failing to do a mapping
  // is acceptable and recoverable from.
  auto retcode = vmaMapMemory(allocator, allocation, &result);
  ETNA_ASSERTF(
    retcode == VK_SUCCESS,
    "Error {} occurred while trying to map an etna::Buffer!",
    vk::to_string(static_cast<vk::Result>(retcode)));

  return mapped = static_cast<std::byte*>(result);
}

void Buffer::unmap()
{
  ETNA_ASSERT(mapped != nullptr);
  vmaUnmapMemory(allocator, allocation);
  mapped = nullptr;
}

BufferBinding Buffer::genBinding(vk::DeviceSize offset, vk::DeviceSize range) const
{
  return BufferBinding{*this, vk::DescriptorBufferInfo{get(), offset, range}};
}

} // namespace etna
