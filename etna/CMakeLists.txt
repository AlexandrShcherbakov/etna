cmake_minimum_required(VERSION 3.20)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
  add_definitions(-DETNA_SET_VULKAN_DEBUG_NAMES)
endif()

add_library(etna
  "source/Image.cpp"
  "source/Buffer.cpp"
  "source/PipelineBase.cpp"
  "source/PipelineManager.cpp"
  "source/VmaImplementation.cpp"
  "source/ShaderProgram.cpp"
  "source/DescriptorSetLayout.cpp"
  "source/GlobalContext.cpp"
  "source/DescriptorSet.cpp"
  "source/VkHppDispatchLoaderStorage.cpp"
  "source/Etna.cpp"
  "source/Sampler.cpp"
  "source/RenderTargetStates.cpp"
  "source/StateTracking.cpp"
  "source/DebugUtils.cpp"
  "source/Window.cpp"
  "source/PerFrameCmdMgr.cpp"
  "source/OneShotCmdMgr.cpp"
  "source/BlockingTransferHelper.cpp")

target_include_directories(etna PUBLIC include)
target_include_directories(etna PRIVATE source)

target_link_libraries(etna PUBLIC
  Vulkan::Vulkan
  VulkanMemoryAllocator
  StbLibraries
  "spirv-reflect-static"
  spdlog::spdlog
  Tracy::TracyClient
)
target_compile_definitions(etna PUBLIC
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_NO_EXCEPTIONS
  TRACY_VK_USE_SYMBOL_TABLE
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=1
)
