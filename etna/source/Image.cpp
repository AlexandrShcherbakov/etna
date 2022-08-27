#include <etna/Image.hpp>


namespace etna
{

Image::Image(CreateInfo info)
  : allocator{info.allocator}
{
  vk::ImageCreateInfo image_info{
    .imageType = vk::ImageType::e2D,
    .format = info.format,
    .extent = info.extent,
    .mipLevels = static_cast<uint32_t>(info.mipLevels),
    .arrayLayers = static_cast<uint32_t>(info.layers),
    .samples = vk::SampleCountFlagBits::e1,
    .tiling = info.tiling,
    .usage = info.imageUsage,
    .sharingMode = vk::SharingMode::eExclusive,
    .initialLayout = vk::ImageLayout::eUndefined
  };
  VmaAllocationCreateInfo alloc_info{
    .usage = info.memoryUsage
  };
  VkImage img;

  auto retcode = vmaCreateImage(allocator, &static_cast<const VkImageCreateInfo&>(image_info), &alloc_info,
      &img, &allocation, nullptr);
  if (retcode != VK_SUCCESS)
  {
    throw std::runtime_error("Unable to create a VMA image!");
  }
  image = img;
}

void Image::swap(Image& other)
{
  std::swap(allocator, other.allocator);
  std::swap(allocation, other.allocation);
  std::swap(image, other.image);
}

Image::Image(Image&& other) noexcept
{
  swap(other);
}

Image& Image::operator =(Image&& other) noexcept
{
  if (this == &other)
  {
    return *this;
  }

  if (image)
  {
    vmaDestroyImage(allocator, image, allocation);
    allocator = {};
    allocation = {};
    image = vk::Image{};
  }

  swap(other);

  return *this;
}

Image::~Image()
{
  if (image)
  {
    vmaDestroyImage(allocator, image, allocation);
  }
}

}
