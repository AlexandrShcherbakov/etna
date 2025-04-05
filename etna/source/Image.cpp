#include <etna/Image.hpp>

#include <etna/GlobalContext.hpp>
#include "DebugUtils.hpp"


namespace etna
{

Image::Image(VmaAllocator alloc, CreateInfo info)
  : allocator{alloc}
  , type{info.type}
  , format{info.format}
  , name{info.name}
  , extent{info.extent}
{
  vk::ImageCreateInfo imageInfo{
    .flags = info.flags,
    .imageType = type,
    .format = format,
    .extent = extent,
    .mipLevels = static_cast<uint32_t>(info.mipLevels),
    .arrayLayers = static_cast<uint32_t>(info.layers),
    .samples = info.samples,
    .tiling = info.tiling,
    .usage = info.imageUsage,
    .sharingMode = vk::SharingMode::eExclusive,
    .initialLayout = vk::ImageLayout::eUndefined,
  };
  VmaAllocationCreateInfo allocInfo{
    .flags = info.allocationCreate,
    .usage = info.memoryUsage,
    .requiredFlags = 0,
    .preferredFlags = 0,
    .memoryTypeBits = 0,
    .pool = nullptr,
    .pUserData = nullptr,
    .priority = 0.f,
  };
  VkImage img;

  auto retcode = vmaCreateImage(
    allocator,
    &static_cast<const VkImageCreateInfo&>(imageInfo),
    &allocInfo,
    &img,
    &allocation,
    nullptr);
  // Note that usually vulkan.hpp handles doing the assertion
  // and a pretty message, but VMA cannot do that.
  ETNA_VERIFYF(
    retcode == VK_SUCCESS,
    "Error {} occurred while trying to allocate an etna::Image!",
    vk::to_string(static_cast<vk::Result>(retcode)));
  image = vk::Image(img);
  etna::set_debug_name(image, name.c_str());
}

void Image::swap(Image& other)
{
  std::swap(views, other.views);
  std::swap(allocator, other.allocator);
  std::swap(allocation, other.allocation);
  std::swap(image, other.image);
  std::swap(type, other.type);
  std::swap(format, other.format);
  std::swap(name, other.name);
  std::swap(extent, other.extent);
}

Image::Image(Image&& other) noexcept
{
  swap(other);
}

Image& Image::operator=(Image&& other) noexcept
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

static vk::ImageAspectFlags get_aspect_mask(vk::Format format)
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

static vk::ImageViewType get_view_type(vk::ImageType image_type)
{
  switch (image_type)
  {
  case vk::ImageType::e1D:
    return vk::ImageViewType::e1D;
  case vk::ImageType::e2D:
    return vk::ImageViewType::e2D;
  case vk::ImageType::e3D:
    return vk::ImageViewType::e3D;
  default:
    return vk::ImageViewType::e2D;
  }
}

vk::ImageAspectFlags Image::getAspectMaskByFormat() const
{
  return get_aspect_mask(format);
}

vk::ImageView Image::getView(Image::ViewParams params) const
{
  auto it = views.find(params);

  if (it == views.end())
  {
    vk::ImageViewCreateInfo viewInfo{
      .image = image,
      .viewType = params.type.value_or(get_view_type(type)),
      .format = params.format.value_or(format),
      .subresourceRange = vk::ImageSubresourceRange{
        .aspectMask = params.aspectMask.value_or(get_aspect_mask(params.format.value_or(format))),
        .baseMipLevel = params.baseMip,
        .levelCount = params.levelCount,
        .baseArrayLayer = params.baseLayer,
        .layerCount = params.layerCount,
      }};
    auto view = unwrap_vk_result(etna::get_context().getDevice().createImageViewUnique(viewInfo));
    set_debug_name(view.get(), name.c_str());
    it = views.emplace(params, std::move(view)).first;
  }

  return views[params].get();
}

ImageBinding Image::genBinding(vk::Sampler sampler, vk::ImageLayout layout, ViewParams params) const
{
  return ImageBinding{*this, vk::DescriptorImageInfo{sampler, getView(params), layout}};
}

} // namespace etna
