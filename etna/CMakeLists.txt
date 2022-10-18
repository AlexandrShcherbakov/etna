cmake_minimum_required(VERSION 3.20)


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
  "source/RenderTargetStates.cpp")

target_include_directories(etna PUBLIC include)
target_include_directories(etna PRIVATE source)

target_link_libraries(etna Vulkan::Vulkan VulkanMemoryAllocator StbLibraries "spirv-reflect-static" spdlog::spdlog)
target_compile_definitions(etna PUBLIC
  VULKAN_HPP_NO_STRUCT_CONSTRUCTORS
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VULKAN_HPP_NO_EXCEPTIONS
)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
  target_compile_definitions(etna PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif (CMAKE_SYSTEM_NAME STREQUAL Linux)
  target_compile_definitions(etna PUBLIC VK_USE_PLATFORM_XLIB_KHR)
elseif (CMAKE_SYSTEM_NAME STREQUAL Darwin)
  target_compile_definitions(etna PUBLIC VK_USE_PLATFORM_MACOS_MVK)
endif ()
