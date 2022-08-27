#pragma once

#include <vulkan/vulkan.hpp>
#include <vk_mem_alloc.h>


namespace etna
{

class Image
{
public:
  Image() = default;

  struct CreateInfo
  {
	VmaAllocator allocator;
	vk::Extent3D extent;
	vk::Format format = vk::Format::eR8G8B8A8Srgb;
	vk::ImageUsageFlags imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
	VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
	std::size_t layers = 1;
	std::size_t mipLevels = 1;
  };

  Image(CreateInfo info);

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  void swap(Image& other);
  Image(Image&&) noexcept;
  Image& operator=(Image&&) noexcept;

  [[nodiscard]] vk::Image get() const { return image; }

  ~Image();

private:
  VmaAllocator allocator{};

  VmaAllocation allocation{};
  vk::Image image{};
};

}
