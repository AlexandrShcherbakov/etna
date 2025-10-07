#include <etna/Window.hpp>

#include <fmt/ranges.h>
#include <tracy/Tracy.hpp>

#include <etna/VulkanFormatter.hpp>
#include <etna/GlobalContext.hpp>

#include "StateTracking.hpp"
#include "DebugUtils.hpp"
#include <etna/GpuWorkCount.hpp>


namespace etna
{

static vk::SurfaceFormatKHR chose_surface_format(
  const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface, bool auto_gamma)
{
  auto formats = unwrap_vk_result(device.getSurfaceFormatsKHR(surface));

  ETNA_VERIFYF(!formats.empty(), "Device does not support any surface formats!");

  auto selected = formats[0];

  const auto desiredFormat = auto_gamma ? vk::Format::eB8G8R8A8Unorm : vk::Format::eB8G8R8A8Srgb;

  auto found = std::find_if(
    formats.begin(), formats.end(), [&desiredFormat](const vk::SurfaceFormatKHR& format) {
      return format.format == desiredFormat &&
        format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
    });

  if (found != formats.end())
    selected = *found;

  return selected;
}

static vk::PresentModeKHR chose_present_mode(
  const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface, bool vsync)
{
  auto modes = unwrap_vk_result(device.getSurfacePresentModesKHR(surface));

  ETNA_VERIFYF(!modes.empty(), "Device doesn't support any present modes!");

  auto selected = vk::PresentModeKHR::eImmediate;

  // NOTE: Fifo is basically v-sync
  if (vsync && std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eFifo) != modes.end())
    selected = vk::PresentModeKHR::eFifo;

  if (!vsync && std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox) != modes.end())
    selected = vk::PresentModeKHR::eMailbox;

  return selected;
}

static vk::Extent2D chose_swap_extent(
  const vk::SurfaceCapabilitiesKHR& capabilities, vk::Extent2D actual_extent)
{
  if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
  {
    return capabilities.currentExtent;
  }

  actual_extent.width = std::max(
    capabilities.minImageExtent.width,
    std::min(capabilities.maxImageExtent.width, actual_extent.width));
  actual_extent.height = std::max(
    capabilities.minImageExtent.height,
    std::min(capabilities.maxImageExtent.height, actual_extent.height));

  return actual_extent;
}


Window::Window(const Dependencies& deps, CreateInfo info)
  : physicalDevice{deps.physicalDevice}
  , device{deps.device}
  , surface(std::move(info.surface))
  , queueFamily{deps.queueFamily}
  , presentQueue{deps.presentQueue}
{
}

std::optional<Window::SwapchainImage> Window::acquireNext()
{
  ZoneScoped;

  if (swapchainInvalid)
    return std::nullopt;
  const auto presentNo = currentSwapchain.presentCounter++;

  auto available =
    currentSwapchain.imageAvailable[presentNo % currentSwapchain.imageAvailable.size()].get();

  // This blocks on mobile when the swapchain has no available images.
  vk::AcquireNextImageInfoKHR info{
    .swapchain = currentSwapchain.swapchain.get(),
    .timeout = 100000000000,
    .semaphore = available,
    .deviceMask = 1,
  };

  uint32_t index;
  const auto res = device.acquireNextImage2KHR(&info, &index);

  if (res == vk::Result::eErrorOutOfDateKHR)
  {
    swapchainInvalid = true;
    return std::nullopt;
  }

  // Theoretically we could recover from this maybe?
  if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR)
    ETNA_PANIC("Swapchain element acquisition failed! Error code {}", vk::to_string(res));

  auto& element = currentSwapchain.elements[index];
  auto readyForPresent = currentSwapchain.imageReadyForPresent[index].get();

  // NOTE: Sometimes swapchain returns the same image twice in a row.
  // This might break stuff, but I'm not sure how right now.

  etna::get_context().getResourceTracker().setExternalTextureState(
    element.image,
    vk::PipelineStageFlagBits2::eColorAttachmentOutput,
    vk::AccessFlagBits2::eNone,
    vk::ImageLayout::eUndefined);

  return SwapchainImage{
    .image = element.image,
    .view = element.image_view.get(),
    .available = available,
    .readyForPresent = readyForPresent,
  };
}

bool Window::present(vk::Semaphore wait, vk::ImageView which)
{
  ZoneScoped;

  ETNA_VERIFYF(
    !swapchainInvalid, "Tried to present to an invalid swapchain! This is unrecoverable!");

  auto index = viewToIdx(which);

  vk::PresentInfoKHR info{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &wait,
    .swapchainCount = 1,
    .pSwapchains = &currentSwapchain.swapchain.get(),
    .pImageIndices = &index,
  };

  auto res = presentQueue.presentKHR(&info);

  if (res == vk::Result::eErrorOutOfDateKHR)
  {
    swapchainInvalid = true;
    return false;
  }
  else if (res == vk::Result::eSuboptimalKHR)
    return false;
  else if (res != vk::Result::eSuccess)
    ETNA_PANIC("Presentation queue submission failed! Error code {}", vk::to_string(res));

  return true;
}

vk::Extent2D Window::recreateSwapchain(const DesiredProperties& props)
{
  ETNA_VERIFY(props.resolution.width != 0 && props.resolution.height != 0);
  currentSwapchain = createSwapchain(props);
  swapchainInvalid = false;

  return currentSwapchain.extent;
}

Window::SwapchainData Window::createSwapchain(const DesiredProperties& props) const
{
  ZoneScoped;

  const auto surfaceCaps =
    unwrap_vk_result(physicalDevice.getSurfaceCapabilitiesKHR(surface.get()));
  const auto format = chose_surface_format(physicalDevice, surface.get(), props.autoGamma);
  const auto presentMode = chose_present_mode(physicalDevice, surface.get(), props.vsync);
  // NOTE: one might think that you can use surfaceCaps.currentExtent instead
  // of all this resolution provider trickery, but no, if you read the vulkan WSI
  // docs close enough, it turns out currentExtent will always be (-1, -1) on
  // wayland and there is nothing we can do about it.
  const auto extent = chose_swap_extent(surfaceCaps, props.resolution);

  // Why + 1 ? see https://vulkan-tutorial.com/Drawing_a_triangle/Presentation/Swap_chain
  std::uint32_t imageCount = surfaceCaps.minImageCount + 1;
  if (surfaceCaps.maxImageCount > 0)
    imageCount = std::min(imageCount, surfaceCaps.maxImageCount);

  SwapchainData newSwapchain;

  // We must have a unique aquire sem for each inflight frame,
  // because we don't want to complicate synchronization with fences.
  // There is no sync between CPU and GPU (because prev line),
  // so if you want to change MAX_FRAMES_INFLIGHT to a higher value,
  // think twice about it!
  const auto numFramesInFlight = props.numFramesInFlight;
  std::uint32_t imageAvailableSemsCount = std::max(imageCount, numFramesInFlight);

  newSwapchain.imageAvailable.resize(imageAvailableSemsCount);
  for (std::size_t i = 0; i < newSwapchain.imageAvailable.size(); ++i)
  {
    newSwapchain.imageAvailable[i] =
      unwrap_vk_result(device.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
    set_debug_name(
      newSwapchain.imageAvailable[i].get(), fmt::format("Swapchain image {} available", i).c_str());
  }

#ifdef linux
  int aboba_ = 0;
  (void)aboba_;
#endif

  // imageReadyForPresent is signaled by queue to present the result,
  // so we need exactly imageCount sems

  newSwapchain.imageReadyForPresent.resize(imageCount);
  for (std::size_t i = 0; i < newSwapchain.imageReadyForPresent.size(); ++i)
  {
    newSwapchain.imageReadyForPresent[i] =
      unwrap_vk_result(device.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
    set_debug_name(
      newSwapchain.imageReadyForPresent[i].get(),
      fmt::format("Swapchain image {} ready for present", i).c_str());
  }

  {
    vk::SwapchainCreateInfoKHR info{
      .surface = surface.get(),
      .minImageCount = imageCount,
      .imageFormat = format.format,
      .imageColorSpace = format.colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferDst,
      .imageSharingMode = vk::SharingMode::eExclusive,
      .queueFamilyIndexCount = 1,
      .pQueueFamilyIndices = &queueFamily,
      .preTransform = surfaceCaps.currentTransform,
      .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
      .presentMode = presentMode,
      .clipped = vk::True,
      .oldSwapchain = currentSwapchain.swapchain.get(),
    };

    newSwapchain.swapchain = unwrap_vk_result(device.createSwapchainKHRUnique(info));
  }

  newSwapchain.format = format.format;
  newSwapchain.extent = extent;

  auto imgs = unwrap_vk_result(device.getSwapchainImagesKHR(newSwapchain.swapchain.get()));

  newSwapchain.elements.reserve(imgs.size());
  for (std::size_t i = 0; i < imgs.size(); ++i)
  {
    auto& el = newSwapchain.elements.emplace_back();
    el.image = imgs[i];
    set_debug_name(el.image, fmt::format("Swapchain element #{}", i).c_str());
    vk::ImageViewCreateInfo info{
      .image = el.image,
      .viewType = vk::ImageViewType::e2D,
      .format = format.format,
      .components = vk::ComponentMapping{},
      .subresourceRange = vk::ImageSubresourceRange{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1}};
    el.image_view = unwrap_vk_result(device.createImageViewUnique(info));
    set_debug_name(el.image_view.get(), fmt::format("Swapchain element #{}", i).c_str());
  }

  return newSwapchain;
}

std::uint32_t Window::viewToIdx(vk::ImageView view)
{
  std::uint32_t index = 0;
  while (index < currentSwapchain.elements.size() &&
         currentSwapchain.elements[index].image_view.get() != view)
  {
    ++index;
  }

  return index;
}

} // namespace etna
