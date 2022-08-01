cmake_minimum_required(VERSION 3.20)


add_library(etna
	"source/UniqueImage.cpp"
 	"source/VmaImplementation.cpp"
 	"source/ShaderProgram.cpp")

target_include_directories(etna PUBLIC include)
target_include_directories(etna PRIVATE source)

target_link_libraries(etna Vulkan::Vulkan VulkanMemoryAllocator StbLibraries SpirvReflect)
