#include <etna/Image.hpp>
#include <etna/GlobalContext.hpp>

namespace etna
{

Image::Image(VmaAllocator alloc, CreateInfo info)
  : allocator{alloc},
  format{info.format}
{
  vk::ImageCreateInfo image_info{
    .imageType = vk::ImageType::e2D,
    .format = format,
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
  image = vk::Image(img);
}

void Image::swap(Image& other)
{
  std::swap(allocator, other.allocator);
  std::swap(allocation, other.allocation);
  std::swap(image, other.image);
  std::swap(format, other.format);
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

  reset();
  swap(other);

  return *this;
}

void Image::reset()
{
  if (image)
  {
    vmaDestroyImage(allocator, VkImage(image), allocation);
    allocator = {};
    allocation = {};
    image = vk::Image{};
    views.clear();
  }
}

Image::~Image()
{
  reset();
}

vk::ImageView Image::getView(Image::ViewParams params) const
{
  if (views.contains(params))
    return views[params];
  vk::ImageViewCreateInfo viewInfo
  {
    .image = image,
    .viewType = vk::ImageViewType::e2D, // TODO: support other types
    .format = format, // TODO: Maybe support anothe type view
    .subresourceRange = vk::ImageSubresourceRange
    {
      .aspectMask = vk::ImageAspectFlagBits::eDepth, // TODO: Remove hardcoded value
      .baseMipLevel = params.baseMip,
      .levelCount = 1, // TODO: get the following params from ViewParams
      .baseArrayLayer = 0,
      .layerCount = 1
    }
  };
  views[params] = etna::get_context().getDevice().createImageView(viewInfo).value;
  return views[params];
}

}
