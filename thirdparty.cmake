cmake_minimum_required(VERSION 3.20)


find_package(Vulkan REQUIRED)

# GPU-side allocator for Vulkan by AMD
CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG master
  DOWNLOAD_ONLY YES
)
if (VulkanMemoryAllocator_ADDED)
  add_library(VulkanMemoryAllocator INTERFACE)
  target_include_directories(VulkanMemoryAllocator INTERFACE ${VulkanMemoryAllocator_SOURCE_DIR}/include/)
endif ()

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
  GIT_TAG master
  OPTIONS
    "SPIRV_REFLECT_EXECUTABLE OFF"
    "SPIRV_REFLECT_STRIPPER OFF"
    "SPIRV_REFLECT_EXAMPLES OFF"
    "SPIRV_REFLECT_BUILD_TESTS OFF"
    "SPIRV_REFLECT_STATIC_LIB ON"
)

# Fmt is a dependency of spdlog, but the version
# included by default has a small but annoying
# bug with latests MSVC versions (19.32+)
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 9.1.0
  OPTIONS
    "FMT_DOC OFF"
    "FMT_INSTALL OFF"
    "FMT_TEST OFF"
)

# Simple logging without headaches
CPMAddPackage(
  NAME spdlog
  GITHUB_REPOSITORY gabime/spdlog
  VERSION 1.10.0
  OPTIONS
    SPDLOG_FMT_EXTERNAL
)
