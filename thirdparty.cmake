cmake_minimum_required(VERSION 3.20)


find_package(Vulkan 1.3.256 REQUIRED)

# GPU-side allocator for Vulkan by AMD
CPMAddPackage(
  NAME VulkanMemoryAllocator
  GITHUB_REPOSITORY GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
  GIT_TAG master
  DOWNLOAD_ONLY YES
)
if (VulkanMemoryAllocator_ADDED)
  add_library(VulkanMemoryAllocator INTERFACE)
  target_include_directories(VulkanMemoryAllocator SYSTEM INTERFACE ${VulkanMemoryAllocator_SOURCE_DIR}/include/)
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

# Type-erased function containers that actually work
CPMAddPackage(
    GITHUB_REPOSITORY Naios/function2
    GIT_TAG 4.2.4
)
