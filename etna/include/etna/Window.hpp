#pragma once
#ifndef ETNA_WINDOW_HPP_INCLUDED
#define ETNA_WINDOW_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/GpuSharedResource.hpp>

#include <function2/function2.hpp>
#include <vk_mem_alloc.h>

namespace etna
{

using ResolutionProvider = fu2::unique_function<vk::Extent2D() const>;

class Window
{
public:
  struct Dependencies
  {
    const GpuWorkCount& workCount;
    vk::PhysicalDevice physicalDevice;
    vk::Device device;

    vk::Queue presentQueue;
    // Reminder: this must be an all-in-one graphics + present queue
    uint32_t queueFamily;
  };

  struct CreateInfo
  {
    vk::UniqueSurfaceKHR surface;
    ResolutionProvider resolutionProvider;
  };

  explicit Window(const Dependencies &deps, CreateInfo info);

  struct SwapchainImage
  {
    vk::Image image;
    vk::ImageView view;
    vk::Semaphore available;
  };

  /**
   * In case this returns a nullopt, swapchain needs recreation at the nearest opportunity.
   * NOTE: might block
   * \returns nullopt when swapchain is out of date and needs to be recreated, next image otherwise
   */
  std::optional<SwapchainImage> acquireNext();

  /**
   * \returns false when swapchain needs to be recreated, true otherwise
   */
  bool present(vk::Semaphore wait, vk::ImageView which);

  std::vector<vk::ImageView> getAllImages();

  vk::Format getCurrentFormat() { return currentSwapchain.format; }

  /**
   * NOTE: might block
   */
  vk::Extent2D recreateSwapchain();

private:
  struct SwapchainElement
  {
    vk::Image image;
    vk::UniqueImageView image_view;
  };
  struct SwapchainData
  {
    vk::UniqueSwapchainKHR swapchain;
    vk::Format format;
    vk::Extent2D extent;
    std::vector<SwapchainElement> elements;
  };

  SwapchainData createSwapchain(vk::Extent2D resolution) const;

  std::uint32_t viewToIdx(vk::ImageView view);

private:
  vk::PhysicalDevice physicalDevice;
  vk::Device device;
  vk::UniqueSurfaceKHR surface;

  ResolutionProvider resolutionProvider;

  std::uint32_t queueFamily;
  vk::Queue presentQueue;

  SwapchainData currentSwapchain{};
  GpuSharedResource<vk::UniqueSemaphore> imageAvailableSem;

  bool swapchainMissing{false};
};

} // namespace etna

#endif // ETNA_WINDOW_HPP_INCLUDED
