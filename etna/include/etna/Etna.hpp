#ifndef ETNA_ETNA_HPP_INCLUDED
#define ETNA_ETNA_HPP_INCLUDED

#include <vulkan/vulkan.hpp>
#include <vector>

#include <etna/ShaderProgram.hpp>
#include <etna/DescriptorSet.hpp>

namespace etna
{
  struct InitParams
  {
    vk::Instance instance {};
    vk::Device device {};
    uint32_t numFramesInFlight;
  };

  bool is_initilized();
  void initialize(const InitParams &params);
  void shutdown();
  
  void submit();

  ShaderProgramId create_program(const std::string &name, const std::vector<std::string> &shaders_path);
  
  /*
    Reload shader files. Warning:
    1) This function must be called from gpu idle state
    2) All descriptor sets become invalid after calling this function
  */
  void reload_shaders();

  ShaderProgramInfo get_shader_program(ShaderProgramId id);
  ShaderProgramInfo get_shader_program(const std::string &name);

  
  DescriptorSet create_descriptor_set(DescriptorLayoutId layout, const vk::ArrayProxy<const Binding> &bindings);

}

#endif