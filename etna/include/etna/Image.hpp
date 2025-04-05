#pragma once
#ifndef ETNA_IMAGE_HPP_INCLUDED
#define ETNA_IMAGE_HPP_INCLUDED

#include <optional>

#include <etna/Vulkan.hpp>
#include <etna/BindingItems.hpp>
#include <vk_mem_alloc.h>


namespace etna
{

class Image
{
public:
  Image() = default;

  // Images are a very versatile tool in graphics and have a TON of settings,
  // which this structure specifies
  struct CreateInfo
  {
    // Size of the image (yes, 3D images are allowed if you specify the correct type)
    vk::Extent3D extent;

    // Name of the image for debugging tools
    std::string_view name;

    // NOTE: this format is the default for TEXTURE ASSETS,
    // if you are using the image as a render target, you almost
    // definitely want UNorm.
    vk::Format format = vk::Format::eR8G8B8A8Srgb;

    // How will this image be used?
    vk::ImageUsageFlags imageUsage =
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    // Basically determines the memory type this texture will live in.
    // You almost definitely almost always want GPU-only memory.
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    // Basically determines the memory properties for the image.
    // You can use CREATE_DEDICATED_MEMORY_BIT for special, big resources,
    // like fullscreen images used as attachments.
    // See recommended usage patterns:
    // https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/usage_patterns.html
    VmaAllocationCreateFlags allocationCreate = 0;

    // Images are stored in "chunks" on the GPU, which is called "optimal tiling".
    // The way these chunks work is platform-dependent and unspecified, so you
    // cannot work with the image's memory without going through Vulkan's APIs.
    // If you chose linear here, the pixels will be stored by rows and so you
    // will be able to correctly interpret the image's memory even without Vulkan,
    // but this will make the GPU work with the image significantly slower.
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;

    // You can do "arrays of images within a single image" by specifying layers > 1
    std::size_t layers = 1;

    // Amount of mip levels for this image.
    // TODO: allow specifying 0 for auto calculation of mips from extent.
    std::size_t mipLevels = 1;

    // HW-supported MSAA stuff, don't touch if you don't know what MSAA is.
    // Note that you are only allowed to specify a single flag here.
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;

    // Type of image: 1D array-of-pixels, simple 2D image or 3D volumetric image.
    vk::ImageType type = vk::ImageType::e2D;

    // Additional flags, primary usage being allowing for creation of cube or array
    // views to array textures (for cube, specify 6 layers).
    vk::ImageCreateFlags flags = {};
  };

  Image(VmaAllocator alloc, CreateInfo info);

  Image(const Image&) = delete;
  Image& operator=(const Image&) = delete;

  void swap(Image& other);
  Image(Image&&) noexcept;
  Image& operator=(Image&&) noexcept;

  [[nodiscard]] vk::Image get() const { return image; }

  ~Image();
  void reset();

  // Parameters for creating views which specify how the image contents should be
  // interpreted and which part of the image we want to "view"
  struct ViewParams
  {
    // First mip levels to view
    uint32_t baseMip = 0;

    // Count of mip levels to view (default = all)
    uint32_t levelCount = vk::RemainingMipLevels;

    // First layer to view
    uint32_t baseLayer = 0;

    // Count of layers to view (default = all)
    uint32_t layerCount = vk::RemainingArrayLayers;

    // "Aspects" of the image to view. Is only useful for combined depth/stencil
    // images, by default is determined automatically as all present aspects.
    std::optional<vk::ImageAspectFlags> aspectMask = std::nullopt;

    // The way we interpret the image: 1/2/3D, cube, array, etc.
    // By default views it as 1/2/3D automatically based on the image itself.
    std::optional<vk::ImageViewType> type = std::nullopt;

    std::optional<vk::Format> format = std::nullopt;

    bool operator==(const ViewParams& b) const = default;
  };
  vk::ImageView getView(ViewParams params) const;

  // Creates a binding to be used with etna::Binding and etna::create_descriptor_set
  ImageBinding genBinding(
    vk::Sampler sampler,
    vk::ImageLayout layout,
    ViewParams params = {0, vk::RemainingMipLevels, 0, vk::RemainingArrayLayers, {}, {}, {}}) const;

  // Returns the "all" aspects combination of flags based on the image's real format
  vk::ImageAspectFlags getAspectMaskByFormat() const;

  vk::Extent3D getExtent() const { return extent; }
  vk::Format getFormat() const { return format; }

private:
  struct ViewParamsHasher
  {
    size_t operator()(ViewParams params) const
    {
      uint32_t hash = 0;
      hashPack(hash, params.baseMip, params.levelCount);
      return hash;
    }

  private:
    template <typename HashT, typename... HashTs>
    inline void hashPack(uint32_t& hash, const HashT& first, HashTs&&... other) const
    {
      auto hasher = std::hash<uint32_t>();
      hash ^= hasher(first) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
      (hashPack(hash, std::forward<HashTs>(other)), ...);
    }
  };
  mutable std::unordered_map<ViewParams, vk::UniqueImageView, ViewParamsHasher> views;
  VmaAllocator allocator{};

  VmaAllocation allocation{};
  vk::Image image{};
  vk::ImageType type;
  vk::Format format;
  std::string name;
  vk::Extent3D extent;
};

} // namespace etna

#endif // ETNA_IMAGE_HPP_INCLUDED
