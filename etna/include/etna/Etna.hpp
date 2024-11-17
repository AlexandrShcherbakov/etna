#pragma once
#ifndef ETNA_ETNA_HPP_INCLUDED
#define ETNA_ETNA_HPP_INCLUDED

#include <optional>
#include <vector>
#include <span>

#include <etna/Vulkan.hpp>
#include <etna/ShaderProgram.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Image.hpp>


namespace etna
{

enum class ValidationLevel : uint32_t
{
  eBasic,
  eExtensive
};

struct InitParams
{
  /// Can be anything
  const char* applicationName;

  /// Use VK_MAKE_VERSION macro
  uint32_t applicationVersion;

  std::span<char const* const> instanceExtensions{};
  std::span<char const* const> deviceExtensions{};

  /// Enable optional features like tessellation via this structure
  vk::PhysicalDeviceFeatures2 features{};

  /// Use this to select a specific GPU or when automatic detection fails
  std::optional<uint32_t> physicalDeviceIndexOverride = std::nullopt;

  /// How much do we allow the CPU to "outrun" the GPU asynchronously
  uint32_t numFramesInFlight = 2;
};

bool is_initilized();
void initialize(const InitParams& params);
void shutdown();

void begin_frame();
void end_frame();

/**
 * \brief Loads shaders and combines them into a shader program. You can then
 * use the shader program by it's name to create pipelines.
 *
 * \param name The name to give this shader program.
 * \param shaders_path Paths to shaders to use in this program.
 * \return ID of the newly created shader program.
 */
ShaderProgramId create_program(
  const char* name, std::initializer_list<std::filesystem::path> shaders_path);

ShaderProgramId get_program_id(const char* name);

/**
 * \brief Reload shader files.
 * \warning
 * 1) This function must be called from gpu idle state
 * 2) All descriptor sets become invalid after calling this function
 */
void reload_shaders();

// Access information required for executing a pipeline.
ShaderProgramInfo get_shader_program(ShaderProgramId id);

// Access information required for executing a pipeline.
ShaderProgramInfo get_shader_program(const char* name);

/**
 * \brief Creates a descriptor set, which basically binds resources to a
 * shader. Also automatically does state transitions barriers for the
 * relevant textures.
 * \note Remember to call etna::flush_barriers before actually using the
 * texture in a draw/dispatch/transfer call!
 *
 * \param layout The layout describing what bindings the target shader has.
 * Use etna::get_shader_program to get it from the shader automatically.
 * \param command_buffer The command buffer that the shader invocation will
 * occur in, i.e. where to record the barriers.
 * \param bindings The table of what to bind where.
 * \return The descriptor set that can then be bound.
 */
DescriptorSet create_descriptor_set(
  DescriptorLayoutId layout, vk::CommandBuffer command_buffer, std::vector<Binding> bindings);

Image create_image_from_bytes(
  Image::CreateInfo info, vk::CommandBuffer command_buffer, const void* data);

/**
 * \brief Sets the state of an image before using it in a certain way.
 * Note that Etna calls this automatically in some cases.
 *
 * \param com_buffer The command buffer being recorded.
 * \param image The image to set the state for.
 * \param pipeline_stage_flags Where will the image be used?
 * \param access_flags How will it be used?
 * \param layout What layout do we want it to be in?
 * \param aspect_flags Which aspects of the image will be used?
 */
void set_state(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlags2 pipeline_stage_flags,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags);

/**
 * \brief Flushes all barriers resulting from set_state calls.
 * \note Remember to call this before any draw/dispatch/transfer commands!
 *
 * \param com_buffer The command buffer being recorded.
 */
void flush_barriers(vk::CommandBuffer com_buffer);

void finish_frame(vk::CommandBuffer com_buffer);

} // namespace etna

#endif // ETNA_ETNA_HPP_INCLUDED
