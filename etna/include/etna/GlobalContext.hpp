#pragma once
#ifndef ETNA_CONTEXT_HPP_INCLUDED
#define ETNA_CONTEXT_HPP_INCLUDED

#include <etna/Vulkan.hpp>
#include <etna/DescriptorSetLayout.hpp>
#include <etna/ShaderProgram.hpp>
#include <etna/DescriptorSet.hpp>
#include <etna/Image.hpp>
#include <etna/Buffer.hpp>

#include <vk_mem_alloc.h>


namespace etna
{
  class GlobalContext
  {
    friend void initialize(const struct InitParams &);

    GlobalContext(const struct InitParams &params);

  public:
    Image createImage(Image::CreateInfo info);
    Buffer createBuffer(Buffer::CreateInfo info);

    vk::Device getDevice() const { return vkDevice.get(); }
    vk::PhysicalDevice getPhysicalDevice() const { return vkPhysDevice; }
    vk::Instance getInstance() const { return vkInstance.get(); }
    vk::Queue getQueue() const { return universalQueue; }
    uint32_t getQueueFamilyIdx() const { return universalQueueFamilyIdx; }

    ShaderProgramManager &getShaderManager() { return shaderPrograms; }
    DescriptorSetLayoutCache &getDescriptorSetLayouts() { return descriptorSetLayouts; }
    DynamicDescriptorPool &getDescriptorPool() { return *descriptorPool; }

    GlobalContext(const GlobalContext&) = delete;
    GlobalContext &operator=(const GlobalContext&) = delete;

  private:
    vk::UniqueInstance vkInstance {};
    vk::UniqueDebugUtilsMessengerEXT vkDebugCallback {};
    vk::PhysicalDevice vkPhysDevice {};
    vk::UniqueDevice vkDevice {};

    // We use a single queue for all purposes.
    // Async compute/transfer is too complicated for demos.
    vk::Queue universalQueue {};
    uint32_t universalQueueFamilyIdx {};

    std::unique_ptr<VmaAllocator_T, void(*)(VmaAllocator)> vmaAllocator{nullptr, nullptr};

    DescriptorSetLayoutCache descriptorSetLayouts {}; 
    ShaderProgramManager shaderPrograms {};
    std::optional<DynamicDescriptorPool> descriptorPool;
  };

  GlobalContext &get_context();
}


#endif