cmake_minimum_required(VERSION 3.25)


find_package(Vulkan 1.3.256 REQUIRED)

# GPU-side allocator for Vulkan by AMD
CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG b8e57472fffa3bd6e0a0b675f4615bf0a823ec4d
)
# VMA headers emit a bunch of warnings >:(
set_property(TARGET VulkanMemoryAllocator PROPERTY SYSTEM TRUE)

# Collection of libraries for loading various image formats
CPMAddPackage(
  NAME StbLibraries
  GITHUB_REPOSITORY nothings/stb
  GIT_TAG master
  DOWNLOAD_ONLY YES
)
if (StbLibraries_ADDED)
  add_library(StbLibraries INTERFACE)
  target_include_directories(StbLibraries INTERFACE ${StbLibraries_SOURCE_DIR}/)
endif ()

# Official library for parsing SPIRV bytecode
CPMAddPackage(
  NAME SpirvReflect
  GITHUB_REPOSITORY KhronosGroup/SPIRV-Reflect
  GIT_TAG main
  OPTIONS
    "SPIRV_REFLECT_EXECUTABLE OFF"
    "SPIRV_REFLECT_STRIPPER OFF"
    "SPIRV_REFLECT_EXAMPLES OFF"
    "SPIRV_REFLECT_BUILD_TESTS OFF"
    "SPIRV_REFLECT_STATIC_LIB ON"
)

# Fmt is a dependency of spdlog, but to be
# safe we explicitly specify our version
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 10.2.1
  OPTIONS
    "FMT_SYSTEM_HEADERS"
    "FMT_DOC OFF"
    "FMT_INSTALL OFF"
    "FMT_TEST OFF"
)

# Simple logging without headaches
CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION 1.13.0
  OPTIONS
    SPDLOG_FMT_EXTERNAL
)

# A profiler for both CPU and GPU
CPMAddPackage(
  GITHUB_REPOSITORY wolfpld/tracy
  GIT_TAG v0.11.1
  OPTIONS
    "TRACY_ON_DEMAND ON"
)
