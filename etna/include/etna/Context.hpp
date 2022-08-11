#pragma once
#ifndef ETNA_CONTEXT_HPP_INCLUDED
#define ETNA_CONTEXT_HPP_INCLUDED

#include <vulkan/vulkan.hpp>

#include "DescriptorSetLayout.hpp"
#include "ShaderProgram.hpp"
#include "DescriptorSet.hpp"

namespace etna
{
  struct GlobalContext
  {
    GlobalContext(vk::Instance instance, vk::Device device)
      : vkInstance {instance}, vkDevice {device},
        descriptorSetLayouts {},
        shaderPrograms {},
        descriptorPool {}
      {}

    vk::Device getDevice() const
    {
      return vkDevice;
    }

    vk::Instance getInstance() const
    {
      return vkInstance;
    }

    ShaderProgramManager &getShaderManager()
    {
      return shaderPrograms;
    }

    DescriptorSetLayoutCache &getDescriptorSetLayouts()
    {
      return descriptorSetLayouts;
    }

    DynamicDescriptorPool &getDescriptorPool()
    {
      return descriptorPool;
    }

    GlobalContext(const GlobalContext&) = delete;
    GlobalContext &operator=(const GlobalContext&) = delete;

  private:
    vk::Instance vkInstance {};
    vk::Device vkDevice {};

    DescriptorSetLayoutCache descriptorSetLayouts {}; 
    ShaderProgramManager shaderPrograms {};
    DynamicDescriptorPool descriptorPool;
  };

  GlobalContext &get_context();
}


#endif