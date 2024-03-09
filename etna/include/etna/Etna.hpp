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
struct InitParams
{
  // Can be anything
  const char* applicationName;
  // Use VK_MAKE_VERSION macro
  uint32_t applicationVersion;

  std::span<char const* const> instanceExtensions{};
  std::span<char const* const> deviceExtensions{};

  // Enable optional features like tessellation via this structure
  vk::PhysicalDeviceFeatures2 features{};

  // Use this if you want to select specific GPU or
  // the automatic detection fails
  std::optional<uint32_t> physicalDeviceIndexOverride = std::nullopt;

  uint32_t numFramesInFlight = 2;
};

bool is_initilized();
void initialize(const InitParams& params);
void shutdown();

void submit();

ShaderProgramId create_program(
  const std::string& name, const std::vector<std::string>& shaders_path);

/*
  Reload shader files. Warning:
  1) This function must be called from gpu idle state
  2) All descriptor sets become invalid after calling this function
*/
void reload_shaders();

ShaderProgramInfo get_shader_program(ShaderProgramId id);
ShaderProgramInfo get_shader_program(const std::string& name);


DescriptorSet create_descriptor_set(
  DescriptorLayoutId layout, vk::CommandBuffer command_buffer, std::vector<Binding> bindings);
Image create_image_from_bytes(
  Image::CreateInfo info, vk::CommandBuffer command_buffer, const void* data);

void set_state(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlagBits2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags);

void finish_frame(vk::CommandBuffer com_buffer);
void flush_barriers(vk::CommandBuffer com_buffer);
} // namespace etna

#endif // ETNA_ETNA_HPP_INCLUDED
