#include <etna/Image.hpp>
#include <etna/GlobalContext.hpp>
#include "DebugUtils.hpp"

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
    .samples = info.samples,
    .tiling = info.tiling,
    .usage = info.imageUsage,
    .sharingMode = vk::SharingMode::eExclusive,
    .initialLayout = vk::ImageLayout::eUndefined
  };
  VmaAllocationCreateInfo alloc_info{
    .flags = 0,
    .usage = info.memoryUsage,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = nullptr,
    .pUserData = nullptr,
    .priority = 0.f
  };
  VkImage img;

  auto retcode = vmaCreateImage(allocator, &static_cast<const VkImageCreateInfo&>(image_info), &alloc_info,
      &img, &allocation, nullptr);
  // Note that usually vulkan.hpp handles doing the assertion
  // and a pretty message, but VMA cannot do that.
  ETNA_ASSERTF(retcode == VK_SUCCESS,
    "Error {} occurred while trying to allocate an etna::Image!",
    vk::to_string(static_cast<vk::Result>(retcode)));
  image = vk::Image(img);
  etna::set_debug_name(image, info.name.data());
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
    return *this;

  reset();
  swap(other);

  return *this;
}

void Image::reset()
{
  if (!image)
    return;
  
  views.clear();
  vmaDestroyImage(allocator, VkImage(image), allocation);
  allocator = {};
  allocation = {};
  image = vk::Image{};
}

Image::~Image()
{
  reset();
}

static vk::ImageAspectFlags get_aspeck_mask(vk::Format format)
{
  switch (format)
  {
  case vk::Format::eS8Uint:
    return vk::ImageAspectFlagBits::eStencil;
  case vk::Format::eD16Unorm:
  case vk::Format::eD32Sfloat:
    return vk::ImageAspectFlagBits::eDepth;
  case vk::Format::eD16UnormS8Uint:
  case vk::Format::eD24UnormS8Uint:
  case vk::Format::eD32SfloatS8Uint:
    return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
  default:
    return vk::ImageAspectFlagBits::eColor;
  }
}

vk::ImageAspectFlags Image::getAspectMaskByFormat() const
{
  return get_aspeck_mask(format);
}

vk::ImageView Image::getView(Image::ViewParams params) const
{
  auto it = views.find(params);
  
  if (it == views.end())
  {
    vk::ImageViewCreateInfo viewInfo
    {
      .image = image,
      .viewType = vk::ImageViewType::e2D, // TODO: support other types
      .format = format, // TODO: Maybe support anothe type view
      .subresourceRange = vk::ImageSubresourceRange
      {
        .aspectMask = params.aspectMask ? params.aspectMask.value() : get_aspeck_mask(format),
        .baseMipLevel = params.baseMip,
        .levelCount = params.levelCount,
        .baseArrayLayer = 0,
        .layerCount = 1
      }
    };
    it = views.emplace(params, etna::get_context().getDevice().createImageViewUnique(viewInfo).value).first;
  }

  return views[params].get();
}

ImageBinding Image::genBinding(vk::Sampler sampler, vk::ImageLayout layout, ViewParams params) const
{
  return ImageBinding{*this, vk::DescriptorImageInfo {sampler, getView(params), layout}};
}

}
