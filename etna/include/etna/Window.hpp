#pragma once
#ifndef ETNA_WINDOW_HPP_INCLUDED
#define ETNA_WINDOW_HPP_INCLUDED

#include <optional>
#include <vector>

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
    std::uint32_t queueFamily;
  };

  struct CreateInfo
  {
    vk::UniqueSurfaceKHR surface;
    ResolutionProvider resolutionProvider;
  };

  explicit Window(const Dependencies& deps, CreateInfo info);

  struct SwapchainImage
  {
    vk::Image image;
    vk::ImageView view;
    vk::Semaphore available;
  };

  /**
   * Acquires the next swapchain image from this window to render a frame into.
   * Blocks when the image is not yet available.
   * \returns nullopt when swapchain is out of date and needs to be recreated, next image otherwise
   */
  std::optional<SwapchainImage> acquireNext();

  /**
   * Presents a swapchain image view acquired from this window to the screen.
   * May block due to vulkan driver wonkyness.
   * \returns false when swapchain needs to be recreated, true otherwise
   */
  bool present(vk::Semaphore wait, vk::ImageView which);

  vk::Format getCurrentFormat() { return currentSwapchain.format; }

  /**
   * Recreates the swapchain and returns it's new resolution.
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
    // NOTE: unlike what some tutorials might say, this does NOT have the same
    // size as work count multi-buffering, and vice-versa, multi-buffering count
    // should NOT be equal to the swap chain image count.
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

  // NOTE: technically, the semaphore is not GPU-CPU shared,
  // as it is a GPU-only synchronization primitive, but due to
  // the way WSI works, it is still kind-of sort-of shared between
  // the OS and the GPU, and so needs to be multi-buffered.
  GpuSharedResource<vk::UniqueSemaphore> imageAvailableSem;

  bool swapchainMissing{false};
};

} // namespace etna

#endif // ETNA_WINDOW_HPP_INCLUDED
