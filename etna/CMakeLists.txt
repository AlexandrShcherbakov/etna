cmake_minimum_required(VERSION 3.20)


add_library(etna
	"source/Image.cpp"
	"source/Buffer.cpp"
 	"source/VmaImplementation.cpp"
 	"source/ShaderProgram.cpp"
	"source/DescriptorSetLayout.cpp")

target_include_directories(etna PUBLIC include)
target_include_directories(etna PRIVATE source)

target_link_libraries(etna Vulkan::Vulkan VulkanMemoryAllocator StbLibraries "spirv-reflect-static")
target_compile_definitions(etna PUBLIC VULKAN_HPP_NO_STRUCT_CONSTRUCTORS)
