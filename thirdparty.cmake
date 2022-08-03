cmake_minimum_required(VERSION 3.20)


find_package(Vulkan REQUIRED)

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

CPMAddPackage(
    NAME StbLibraries
    GITHUB_REPOSITORY nothings/stb
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

CPMAddPackage(
    NAME SpirvReflect
    GITHUB_REPOSITORY KhronosGroup/SPIRV-Reflect
    GIT_TAG master
    DOWNLOAD_ONLY YES
)

if (StbLibraries_ADDED)
    add_library(StbLibraries INTERFACE)
    target_include_directories(StbLibraries INTERFACE ${StbLibraries_SOURCE_DIR}/)
endif ()

if (SpirvReflect_ADDED)
    add_library(SpirvReflect ${SpirvReflect_SOURCE_DIR}/spirv_reflect.cpp)
    target_include_directories(SpirvReflect INTERFACE ${SpirvReflect_SOURCE_DIR}/)
endif ()
