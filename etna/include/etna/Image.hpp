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

  struct CreateInfo
  {
    vk::Extent3D extent;
    std::string_view name;
    // NOTE: this format is the default for TEXTURE ASSETS,
    // if you are using the image as a render target, you almost
    // definitely want UNorm.
    vk::Format format = vk::Format::eR8G8B8A8Srgb;
    vk::ImageUsageFlags imageUsage =
      vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;
    std::size_t layers = 1;
    std::size_t mipLevels = 1;
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1;
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

  struct ViewParams
  {
    uint32_t baseMip = 0;
    uint32_t levelCount = 1;
    std::optional<vk::ImageAspectFlagBits> aspectMask{};

    bool operator==(const ViewParams& b) const = default;
  };
  vk::ImageView getView(ViewParams params) const;

  ImageBinding genBinding(
    vk::Sampler sampler, vk::ImageLayout layout, ViewParams params = {0, 1, {}}) const;

  vk::ImageAspectFlags getAspectMaskByFormat() const;

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
  vk::Format format;
  std::string name;
};

} // namespace etna

#endif // ETNA_IMAGE_HPP_INCLUDED
