#include <etna/Window.hpp>


namespace etna
{

static vk::SurfaceFormatKHR chose_surface_format(
  const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
  auto formats = unwrap_vk_result(device.getSurfaceFormatsKHR(surface));

  ETNA_ASSERTF(!formats.empty(), "Device does not support any surface formats!");

  auto found = std::find_if(formats.begin(), formats.end(), [](const vk::SurfaceFormatKHR& format) {
    return format.format == vk::Format::eB8G8R8A8Srgb &&
      format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
  });

  return found != formats.end() ? *found : formats[0];
}

static vk::PresentModeKHR chose_present_mode(
  const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface)
{
  auto modes = unwrap_vk_result(device.getSurfacePresentModesKHR(surface));

  ETNA_ASSERTF(!modes.empty(), "Device doesn't support any present modes!");

  return std::find(modes.begin(), modes.end(), vk::PresentModeKHR::eMailbox) != modes.end()
    ? vk::PresentModeKHR::eMailbox
    : vk::PresentModeKHR::eFifo;
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
  , resolutionProvider{std::move(info.resolutionProvider)}
  , queueFamily{deps.queueFamily}
  , presentQueue{deps.presentQueue}
  , imageAvailableSem(deps.workCount, [&deps](std::size_t) {
    return unwrap_vk_result(deps.device.createSemaphoreUnique(vk::SemaphoreCreateInfo{}));
  })
{
}

std::optional<Window::SwapchainImage> Window::acquireNext()
{
  auto sem = imageAvailableSem.get().get();

  // This blocks on mobile when the swapchain has no available images.
  vk::AcquireNextImageInfoKHR info{
    .swapchain = currentSwapchain.swapchain.get(),
    .timeout = 100000000000,
    .semaphore = sem,
    .deviceMask = 1,
  };
  uint32_t index;
  const auto res = device.acquireNextImage2KHR(&info, &index);

  if (res == vk::Result::eErrorOutOfDateKHR)
    return std::nullopt;

  // Theoretically we could recover from this maybe?
  if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR)
    ETNA_PANIC("Swapchain element acquisition failed! Error code {}", vk::to_string(res));

  auto& element = currentSwapchain.elements[index];

  // NOTE: Sometimes swapchain returns the same image twice in a row. This might break
  // stuff, but I'm not sure how right now.

  return SwapchainImage{
    .image = element.image,
    .view = element.image_view.get(),
    .available = sem,
  };
}

bool Window::present(vk::Semaphore wait, vk::ImageView which)
{
  auto index = viewToIdx(which);

  vk::PresentInfoKHR info{
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = &wait,
    .swapchainCount = 1,
    .pSwapchains = &currentSwapchain.swapchain.get(),
    .pImageIndices = &index,
  };

  auto res = presentQueue.presentKHR(&info);

  if (res == vk::Result::eErrorOutOfDateKHR || res == vk::Result::eSuboptimalKHR)
    return false;
  else if (res != vk::Result::eSuccess)
    ETNA_PANIC("Presentation queue submission failed! Error code {}", vk::to_string(res));

  return true;
}

std::vector<vk::ImageView> Window::getAllImages()
{
  std::vector<vk::ImageView> result;
  result.reserve(currentSwapchain.elements.size());
  for (auto& el : currentSwapchain.elements)
  {
    result.push_back(el.image_view.get());
  }

  return result;
}

vk::Extent2D Window::recreateSwapchain()
{
  // When the window gets minimized, we have to wait (resolution provider blocks)
  auto resolution = resolutionProvider();
  currentSwapchain = createSwapchain(resolution);

  return currentSwapchain.extent;
}

Window::SwapchainData Window::createSwapchain(vk::Extent2D resolution) const
{
  const auto surfaceCaps =
    unwrap_vk_result(physicalDevice.getSurfaceCapabilitiesKHR(surface.get()));
  const auto format = chose_surface_format(physicalDevice, surface.get());
  const auto presentMode = chose_present_mode(physicalDevice, surface.get());
  const auto extent = chose_swap_extent(surfaceCaps, resolution);

  std::uint32_t imageCount = surfaceCaps.minImageCount + 1;

  if (surfaceCaps.maxImageCount > 0)
  {
    imageCount = std::min(imageCount, surfaceCaps.maxImageCount);
  }

  SwapchainData newSwapchain;

  vk::SwapchainCreateInfoKHR info{
    .surface = surface.get(),
    .minImageCount = imageCount,
    .imageFormat = format.format,
    .imageColorSpace = format.colorSpace,
    .imageExtent = extent,
    .imageArrayLayers = 1,
    .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
    .imageSharingMode = vk::SharingMode::eExclusive,
    .queueFamilyIndexCount = 1,
    .pQueueFamilyIndices = &queueFamily,
    .preTransform = surfaceCaps.currentTransform,
    .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
    .presentMode = presentMode,
    .clipped = vk::True,
    .oldSwapchain = currentSwapchain.swapchain.get()};

  newSwapchain.swapchain = unwrap_vk_result(device.createSwapchainKHRUnique(info));

  newSwapchain.format = format.format;
  newSwapchain.extent = extent;

  auto imgs = unwrap_vk_result(device.getSwapchainImagesKHR(newSwapchain.swapchain.get()));

  newSwapchain.elements.reserve(imgs.size());
  for (auto img : imgs)
  {
    auto& el = newSwapchain.elements.emplace_back();
    el.image = img;
    vk::ImageViewCreateInfo info{
      .image = img,
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
