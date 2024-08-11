#include <etna/GlobalContext.hpp>

#include <unordered_set>
#include <spdlog/fmt/ranges.h>
#include <vulkan/vulkan_structs.hpp>

#include <etna/Etna.hpp>
#include <etna/DescriptorSetLayout.hpp>
#include <etna/ShaderProgram.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Assert.hpp>
#include <etna/EtnaConfig.hpp>
#include <etna/Window.hpp>
#include <etna/PerFrameCmdMgr.hpp>
#include <etna/OneShotCmdMgr.hpp>

#include "StateTracking.hpp"


namespace etna
{

static vk::UniqueInstance createInstance(const InitParams& params)
{
  vk::ApplicationInfo appInfo{
    .pApplicationName = params.applicationName,
    .applicationVersion = params.applicationVersion,
    .pEngineName = ETNA_ENGINE_NAME,
    .engineVersion = ETNA_VERSION,
    .apiVersion = VULKAN_API_VERSION,
  };

  std::vector<const char*> extensions(
    params.instanceExtensions.begin(), params.instanceExtensions.end());
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

  std::vector<const char*> layers(VALIDATION_LAYERS.begin(), VALIDATION_LAYERS.end());
  // Compatibility layer for devices that do not implement this extension natively.
  // Sync2 provides potential for driver optimization and a saner programmer API,
  // but is able to be translated into old synchronization calls if needed.
  layers.push_back("VK_LAYER_KHRONOS_synchronization2");

  vk::InstanceCreateInfo createInfo{
    .pApplicationInfo = &appInfo,
  };

  createInfo.setPEnabledLayerNames(layers);
  createInfo.setPEnabledExtensionNames(extensions);

  return vk::createInstanceUnique(createInfo).value;
}

static bool checkPhysicalDeviceSupportsExtensions(
  vk::PhysicalDevice pdevice, std::span<char const* const> extensions)
{
  std::vector availableExtensions = pdevice.enumerateDeviceExtensionProperties().value;

  std::unordered_set requestedExtensions(extensions.begin(), extensions.end());
  for (const auto& ext : availableExtensions)
    requestedExtensions.erase(ext.extensionName);

  return requestedExtensions.empty();
}

static bool deviceTypeIsBetter(vk::PhysicalDeviceType first, vk::PhysicalDeviceType second)
{
  auto score = [](vk::PhysicalDeviceType type) {
    switch (type)
    {
    case vk::PhysicalDeviceType::eVirtualGpu:
      return 1;
    case vk::PhysicalDeviceType::eIntegratedGpu:
      return 2;
    case vk::PhysicalDeviceType::eDiscreteGpu:
      return 3;
    default:
      return 0;
    }
  };
  return score(first) > score(second);
}

vk::PhysicalDevice pick_physical_device(vk::Instance instance, const InitParams& params)
{
  std::vector pdevices = instance.enumeratePhysicalDevices().value;

  ETNA_ASSERTF(!pdevices.empty(), "This PC has no GPUs that support Vulkan!");

  {
    std::vector<std::string> pdeviceNames;
    pdeviceNames.reserve(pdevices.size());
    for (auto pdevice : pdevices)
    {
      const auto props = pdevice.getProperties();
      // TODO: potential bug, is deviceName guaranteed to be null-terminated?
      pdeviceNames.emplace_back(props.deviceName.data());
    }
    spdlog::info("List of physical devices: {}", pdeviceNames);
  }

  if (params.physicalDeviceIndexOverride)
  {
    ETNA_ASSERTF(
      *params.physicalDeviceIndexOverride < pdevices.size(),
      "There's no device with index {}!",
      *params.physicalDeviceIndexOverride);

    auto pdevice = pdevices[*params.physicalDeviceIndexOverride];

    if (checkPhysicalDeviceSupportsExtensions(pdevice, params.deviceExtensions))
    {
      return pdevice;
    }

    spdlog::error(
      "Chosen physical device override '{}' does"
      " not support requested extensions! Falling back to automatic"
      " device selection.",
      std::string_view{pdevice.getProperties().deviceName});
  }

  auto bestDevice = pdevices.front();
  auto bestDeviceProps = pdevices.front().getProperties();
  for (auto pdevice : pdevices)
  {
    auto props = pdevice.getProperties();

    if (!checkPhysicalDeviceSupportsExtensions(pdevice, params.deviceExtensions))
      continue;

    if (deviceTypeIsBetter(props.deviceType, bestDeviceProps.deviceType))
    {
      bestDevice = pdevice;
      bestDeviceProps = props;
    }
  }

  return bestDevice;
}

static uint32_t get_queue_family_index(vk::PhysicalDevice pdevice, vk::QueueFlags flags)
{
  std::vector queueFamilies = pdevice.getQueueFamilyProperties();

  for (uint32_t i = 0; i < queueFamilies.size(); ++i)
  {
    const auto& props = queueFamilies[i];

    if (props.queueCount > 0 && (props.queueFlags & flags))
      return i;
  }

  ETNA_PANIC("Could not find a queue family that supports all requested flags!");
}

static vk::UniqueDevice createDevice(
  vk::PhysicalDevice pdevice, uint32_t universal_queue_family, const InitParams& params)
{
  const float defaultQueuePriority{0.0f};

  // For now we use a single universal queue for everything.
  // Also, it's up to the framework to decide what queueus it needs and supports.

  const std::array queueInfos{
    vk::DeviceQueueCreateInfo{
      .queueFamilyIndex = universal_queue_family,
      .queueCount = 1,
      .pQueuePriorities = &defaultQueuePriority,
    },
  };

  vk::PhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{
    // Evil const cast due to C not having const
    .pNext = const_cast<vk::PhysicalDeviceFeatures2*>(&params.features), // NOLINT
    .dynamicRendering = VK_TRUE,
  };

  vk::PhysicalDeviceSynchronization2Features sync2Feature{
    .pNext = &dynamicRenderingFeature,
    .synchronization2 = VK_TRUE,
  };

  std::vector<char const*> deviceExtensions(
    params.deviceExtensions.begin(), params.deviceExtensions.end());
  deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
#ifdef ETNA_SET_VULKAN_DEBUG_NAMES
  deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
#endif


  // NOTE: original design of PhysicalDeviceFeatures did not
  // support extensions, so they had to use a trick to achieve
  // extensions support. Now pNext has to point to a
  // PhysicalDeviceFeatures2 structure while the actual
  // pEnabledFeatures has to be nullptr.
  return pdevice
    .createDeviceUnique(vk::DeviceCreateInfo{
      .pNext = &sync2Feature,
      .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
      .pQueueCreateInfos = queueInfos.data(),
      .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.data(),
    })
    .value;
}

#ifndef NDEBUG
static VkBool32 debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
  const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
  void* /*pUserData*/)
{
  if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0)
  {
    spdlog::error(callback_data->pMessage);
  }
  else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0)
  {
    spdlog::warn(callback_data->pMessage);
  }
  else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0)
  {
    spdlog::info(callback_data->pMessage);
  }
  else
  {
    spdlog::trace(callback_data->pMessage);
  }

  return VK_FALSE;
}
#endif

GlobalContext::GlobalContext(const InitParams& params)
  : mainWorkStream{params.numFramesInFlight}
{
  // Proper initialization of vulkan is tricky, as we need to
  // dynamically link vulkan-1.dll and load symbols for various
  // extensions at runtime. Moreover, extensions can be device
  // specific and API version specific, so symbol loading is
  // done in 3 steps:
  // 1) load version-independent symbols
  // 2) load device-independent symbols
  // 3) load device-specific symbols
  vk::DynamicLoader dl;
  VULKAN_HPP_DEFAULT_DISPATCHER.init(
    dl.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

  vkInstance = createInstance(params);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkInstance.get());

// NOTE: Previously we used VK_EXT_debug_report extension,
// but it is considered to be abandoned in favour of
// VK_EXT_debug_utils.
#ifndef NDEBUG
  {
    vk::DebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo{
      .messageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning,
      .messageType = vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation,
      .pfnUserCallback = debugCallback,
      .pUserData = this,
    };
    vkDebugCallback = vkInstance->createDebugUtilsMessengerEXTUnique(debugUtilsCreateInfo).value;
  }
#endif

  vkPhysDevice = pick_physical_device(vkInstance.get(), params);


  constexpr auto UNIVERSAL_QUEUE_FLAGS =
    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
  universalQueueFamilyIdx = get_queue_family_index(vkPhysDevice, UNIVERSAL_QUEUE_FLAGS);
  vkDevice = createDevice(vkPhysDevice, universalQueueFamilyIdx, params);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkDevice.get());

  universalQueue = vkDevice->getQueue(universalQueueFamilyIdx, 0);

  {
    VmaAllocatorCreateInfo allocInfo{
      .flags = {},
      .physicalDevice = vkPhysDevice,
      .device = vkDevice.get(),

      .preferredLargeHeapBlockSize = {},
      .pAllocationCallbacks = {},
      .pDeviceMemoryCallbacks = {},
      .pHeapSizeLimit = {},
      .pVulkanFunctions = {},

      .instance = vkInstance.get(),
      .vulkanApiVersion = VULKAN_API_VERSION,
      .pTypeExternalMemoryHandleTypes = nullptr,
    };

    VmaAllocator allocator;
    ::vmaCreateAllocator(&allocInfo, &allocator);

    vmaAllocator = {allocator, &::vmaDestroyAllocator};
  }

  descriptorSetLayouts = std::make_unique<DescriptorSetLayoutCache>();
  shaderPrograms = std::make_unique<ShaderProgramManager>();
  pipelineManager = std::make_unique<PipelineManager>(vkDevice.get(), *shaderPrograms);
  descriptorPool = std::make_unique<DynamicDescriptorPool>(vkDevice.get(), mainWorkStream);
  resourceTracking = std::make_unique<ResourceStates>();
}

Image GlobalContext::createImage(const Image::CreateInfo& info)
{
  return Image(vmaAllocator.get(), info);
}

Buffer GlobalContext::createBuffer(const Buffer::CreateInfo& info)
{
  return Buffer(vmaAllocator.get(), info);
}

std::unique_ptr<Window> GlobalContext::createWindow(Window::CreateInfo info)
{
  Window::Dependencies deps{
    .workCount = mainWorkStream,
    .physicalDevice = vkPhysDevice,
    .device = vkDevice.get(),
    .presentQueue = universalQueue,
    .queueFamily = universalQueueFamilyIdx};
  return std::make_unique<Window>(deps, std::move(info));
}

std::unique_ptr<PerFrameCmdMgr> GlobalContext::createPerFrameCmdMgr()
{
  PerFrameCmdMgr::Dependencies deps{
    .workCount = mainWorkStream,
    .device = vkDevice.get(),
    .submitQueue = universalQueue,
    .queueFamily = universalQueueFamilyIdx};
  return std::make_unique<PerFrameCmdMgr>(deps);
}

std::unique_ptr<OneShotCmdMgr> GlobalContext::createOneShotCmdMgr()
{
  OneShotCmdMgr::Dependencies deps{
    .device = vkDevice.get(),
    .submitQueue = universalQueue,
    .queueFamily = universalQueueFamilyIdx};
  return std::make_unique<OneShotCmdMgr>(deps);
}

ShaderProgramManager& GlobalContext::getShaderManager()
{
  return *shaderPrograms;
}

PipelineManager& GlobalContext::getPipelineManager()
{
  return *pipelineManager;
}

DescriptorSetLayoutCache& GlobalContext::getDescriptorSetLayouts()
{
  return *descriptorSetLayouts;
}

DynamicDescriptorPool& GlobalContext::getDescriptorPool()
{
  return *descriptorPool;
}

ResourceStates& GlobalContext::getResourceTracker()
{
  return *resourceTracking;
}

GlobalContext::~GlobalContext() = default;

} // namespace etna
