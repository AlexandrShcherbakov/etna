#include <etna/GlobalContext.hpp>

#include <unordered_set>
#include <spdlog/fmt/ranges.h>
#include <tracy/TracyVulkan.hpp>

#include <etna/Etna.hpp>
#include <etna/DescriptorSetLayout.hpp>
#include <etna/ShaderProgram.hpp>
#include <etna/PipelineManager.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Assert.hpp>
#include <etna/EtnaConfig.hpp>
#include <etna/EtnaEngineConfig.hpp>
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
    .engineVersion = ETNA_ENGINE_VERSION,
    .apiVersion = VULKAN_API_VERSION,
  };

  std::vector<const char*> extensions(
    params.instanceExtensions.begin(), params.instanceExtensions.end());
  extensions.push_back(vk::EXTDebugUtilsExtensionName);

  // NOTE: Extension for the vulkan loader to list non-conformant implementations, such as
  // for example MoltenVK on Apple devices.
#if defined(__APPLE__)
  extensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
#endif

  std::vector<const char*> layers(VULKAN_LAYERS.begin(), VULKAN_LAYERS.end());

  vk::InstanceCreateInfo createInfo{
    .pApplicationInfo = &appInfo,
  };

  // NOTE: Enable non-conformant Vulkan implementations.
#if defined(__APPLE__)
  createInfo.setFlags(vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR);
#endif

  createInfo.setPEnabledLayerNames(layers);
  createInfo.setPEnabledExtensionNames(extensions);

  spdlog::info(
    "Creating a Vulkan instance with the following extensions and layers: {}; {}",
    extensions,
    layers);

  return unwrap_vk_result(vk::createInstanceUnique(createInfo));
}

template <std::size_t SIZE>
static std::string_view safe_view_of_array(const vk::ArrayWrapper1D<char, SIZE>& array)
{
  // Paranoic strlen
  const std::size_t length =
    static_cast<const char*>(std::memchr(array.data(), '\0', SIZE)) - array.data();
  return std::string_view{array.data(), length};
}

static bool check_physical_device_supports_extensions(
  vk::PhysicalDevice pdevice, std::span<char const* const> extensions)
{
  std::vector availableExtensions = unwrap_vk_result(pdevice.enumerateDeviceExtensionProperties());

  std::unordered_set<std::string_view> requestedExtensions(extensions.begin(), extensions.end());
  for (const auto& ext : availableExtensions)
    requestedExtensions.erase(safe_view_of_array(ext.extensionName));

  return requestedExtensions.empty();
}

struct OptionalExtensionsFound
{
  bool hasVkExtCalibratedTimestamps = false;
};

static OptionalExtensionsFound collect_optional_extensions_to_use(vk::PhysicalDevice pdevice)
{
  const std::vector availableExtensions =
    unwrap_vk_result(pdevice.enumerateDeviceExtensionProperties());

  OptionalExtensionsFound result;

  for (const auto& ext : availableExtensions)
  {
    if (
      safe_view_of_array(ext.extensionName) ==
      std::string_view(vk::KHRCalibratedTimestampsExtensionName))
      result.hasVkExtCalibratedTimestamps = true;
  }

  return result;
}

static bool device_type_is_better(vk::PhysicalDeviceType first, vk::PhysicalDeviceType second)
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
  std::vector pdevices = unwrap_vk_result(instance.enumeratePhysicalDevices());

  ETNA_VERIFYF(!pdevices.empty(), "This PC has no GPUs that support Vulkan!");

  {
    std::vector<std::string> pdeviceNames;
    pdeviceNames.reserve(pdevices.size());
    for (auto pdevice : pdevices)
    {
      const auto props = pdevice.getProperties();
      pdeviceNames.emplace_back(safe_view_of_array(props.deviceName));
    }
    spdlog::info("List of physical devices: {}", pdeviceNames);
  }

  if (params.physicalDeviceIndexOverride)
  {
    ETNA_VERIFYF(
      *params.physicalDeviceIndexOverride < pdevices.size(),
      "There's no device with index {}!",
      *params.physicalDeviceIndexOverride);

    auto pdevice = pdevices[*params.physicalDeviceIndexOverride];

    if (check_physical_device_supports_extensions(pdevice, params.deviceExtensions))
    {
      return pdevice;
    }

    spdlog::error(
      "Chosen physical device override '{}' does"
      " not support requested extensions! Falling back to automatic"
      " device selection.",
      safe_view_of_array(pdevice.getProperties().deviceName));
  }

  auto bestDevice = pdevices.front();
  auto bestDeviceProps = pdevices.front().getProperties();
  for (auto pdevice : pdevices)
  {
    auto props = pdevice.getProperties();

    if (!check_physical_device_supports_extensions(pdevice, params.deviceExtensions))
      continue;

    if (device_type_is_better(props.deviceType, bestDeviceProps.deviceType))
    {
      bestDevice = pdevice;
      bestDeviceProps = props;
    }
  }

  spdlog::info("Chosing physical device {}", safe_view_of_array(bestDeviceProps.deviceName));

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

static vk::UniqueDevice create_logical_device(
  vk::PhysicalDevice pdevice,
  uint32_t universal_queue_family,
  const InitParams& params,
  const OptionalExtensionsFound& optional_exts)
{
  const float defaultQueuePriority{0.0f};

  // For now we use a single universal queue for everything.
  // Also, it's up to the framework to decide what queues it needs and supports.

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
    .dynamicRendering = vk::True,
  };

  vk::PhysicalDeviceSynchronization2Features sync2Feature{
    .pNext = &dynamicRenderingFeature,
    .synchronization2 = vk::True,
  };

  std::vector<char const*> deviceExtensions(
    params.deviceExtensions.begin(), params.deviceExtensions.end());

  if (optional_exts.hasVkExtCalibratedTimestamps)
  {
    deviceExtensions.push_back(vk::KHRCalibratedTimestampsExtensionName);
  }

  // NOTE: These extensions are needed on MoltenVK to be set explicitly due to
  // it not fully supporting Vulkan 1.3 yet.
#if defined(__APPLE__)
  deviceExtensions.push_back(vk::KHRDynamicRenderingExtensionName);
  deviceExtensions.push_back(vk::KHRSynchronization2ExtensionName);
  deviceExtensions.push_back(vk::KHRCopyCommands2ExtensionName);

  // NOTE: Enable non-conformant Vulkan implementations.
  deviceExtensions.push_back(vk::KHRPortabilitySubsetExtensionName);
#endif

  // NOTE: original design of PhysicalDeviceFeatures did not
  // support extensions, so they had to use a trick to achieve
  // extensions support. Now pNext has to point to a
  // PhysicalDeviceFeatures2 structure while the actual
  // pEnabledFeatures has to be nullptr.
  vk::DeviceCreateInfo creatInfo{
    .pNext = &sync2Feature,
    .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
    .pQueueCreateInfos = queueInfos.data(),
    .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
    .ppEnabledExtensionNames = deviceExtensions.data(),
  };

  spdlog::info("Creating a logical device with the following extensions: {}", deviceExtensions);

  return unwrap_vk_result(pdevice.createDeviceUnique(creatInfo));
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
  , tracyCtx{nullptr, +[](void* ctx) {
               (void)ctx;
               TracyVkDestroy(reinterpret_cast<TracyVkCtx>(ctx));
             }}
  , shouldGenerateBarriersFlag{params.generateBarriersAutomatically}
{
  // Proper initialization of vulkan is tricky, as we need to
  // dynamically link vulkan-1.dll and load symbols for various
  // extensions at runtime. Moreover, extensions can be device
  // specific and API version specific, so symbol loading is
  // done in 3 steps:
  // 1) load version-independent symbols
  // 2) load device-independent symbols
  // 3) load device-specific symbols
  VULKAN_HPP_DEFAULT_DISPATCHER.init(
    vkDynamicLoader.getProcAddress<PFN_vkGetInstanceProcAddr>("vkGetInstanceProcAddr"));

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
    vkDebugCallback =
      unwrap_vk_result(vkInstance->createDebugUtilsMessengerEXTUnique(debugUtilsCreateInfo));
  }
#endif

  vkPhysDevice = pick_physical_device(vkInstance.get(), params);

  const auto optionalExts = collect_optional_extensions_to_use(vkPhysDevice);

  constexpr auto UNIVERSAL_QUEUE_FLAGS =
    vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
  universalQueueFamilyIdx = get_queue_family_index(vkPhysDevice, UNIVERSAL_QUEUE_FLAGS);
  vkDevice = create_logical_device(vkPhysDevice, universalQueueFamilyIdx, params, optionalExts);
  VULKAN_HPP_DEFAULT_DISPATCHER.init(vkDevice.get());

  universalQueue = vkDevice->getQueue(universalQueueFamilyIdx, 0);

  {
    VmaVulkanFunctions functions{};
    functions.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocInfo{
      .flags = {},
      .physicalDevice = vkPhysDevice,
      .device = vkDevice.get(),

      .preferredLargeHeapBlockSize = {},
      .pAllocationCallbacks = {},
      .pDeviceMemoryCallbacks = {},
      .pHeapSizeLimit = {},
      .pVulkanFunctions = &functions,

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

  auto tempPool =
    etna::unwrap_vk_result(vkDevice->createCommandPoolUnique(vk::CommandPoolCreateInfo{
      .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
      .queueFamilyIndex = universalQueueFamilyIdx,
    }));
  auto buf = std::move(
    etna::unwrap_vk_result(vkDevice->allocateCommandBuffersUnique(vk::CommandBufferAllocateInfo{
      .commandPool = tempPool.get(),
      .level = vk::CommandBufferLevel::ePrimary,
      .commandBufferCount = 1,
    }))[0]);

  // Workaround for issues in Tracy =(
#ifdef TRACY_ENABLE
  // TODO: support TracyVkContextHostCalibrated
  if (optionalExts.hasVkExtCalibratedTimestamps)
  {
    auto ctx = TracyVkContextCalibrated(
      vkInstance.get(),
      vkPhysDevice,
      vkDevice.get(),
      universalQueue,
      buf.get(),
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
    tracyCtx.reset(ctx);
  }
  else
  {
    auto ctx = TracyVkContext(
      vkInstance.get(),
      vkPhysDevice,
      vkDevice.get(),
      universalQueue,
      buf.get(),
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr,
      VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr);
    tracyCtx.reset(ctx);
  }
#endif
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


bool GlobalContext::shouldGenerateBarriersWhen(BarrierBehavior behavior) const
{
  if (behavior == BarrierBehavior::eDefault)
  {
    return shouldGenerateBarriersFlag;
  }
  return behavior == BarrierBehavior::eGenerateBarriers;
}

} // namespace etna
