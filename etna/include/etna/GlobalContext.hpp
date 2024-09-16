#pragma once
#ifndef ETNA_GLOBAL_CONTEXT_HPP_INCLUDED
#define ETNA_GLOBAL_CONTEXT_HPP_INCLUDED

#include <memory>

#include <etna/Vulkan.hpp>
#include <etna/GpuWorkCount.hpp>
#include <etna/Image.hpp>
#include <etna/Buffer.hpp>
#include <etna/Window.hpp>

#include <vk_mem_alloc.h>


namespace etna
{

struct DescriptorSetLayoutCache;
struct ShaderProgramManager;
class PipelineManager;
struct DynamicDescriptorPool;
class ResourceStates;
class PerFrameCmdMgr;
class OneShotCmdMgr;

class GlobalContext
{
  friend void initialize(const struct InitParams&);

  explicit GlobalContext(const struct InitParams& params);

public:
  Image createImage(const Image::CreateInfo& info);
  Buffer createBuffer(const Buffer::CreateInfo& info);
  std::unique_ptr<Window> createWindow(Window::CreateInfo info);
  std::unique_ptr<PerFrameCmdMgr> createPerFrameCmdMgr();
  std::unique_ptr<OneShotCmdMgr> createOneShotCmdMgr();

  vk::Device getDevice() const { return vkDevice.get(); }
  vk::PhysicalDevice getPhysicalDevice() const { return vkPhysDevice; }
  vk::Instance getInstance() const { return vkInstance.get(); }
  vk::Queue getQueue() const { return universalQueue; }
  uint32_t getQueueFamilyIdx() const { return universalQueueFamilyIdx; }

  ShaderProgramManager& getShaderManager();
  PipelineManager& getPipelineManager();
  DescriptorSetLayoutCache& getDescriptorSetLayouts();
  DynamicDescriptorPool& getDescriptorPool();
  ResourceStates& getResourceTracker();
  GpuWorkCount& getMainWorkCount() { return mainWorkStream; }
  const GpuWorkCount& getMainWorkCount() const { return mainWorkStream; }

  // Do not use this directly, use Profiling.hpp
  void* getTracyContext() { return tracyCtx.get(); }

  GlobalContext(const GlobalContext&) = delete;
  GlobalContext& operator=(const GlobalContext&) = delete;
  GlobalContext(GlobalContext&&) = delete;
  GlobalContext& operator=(GlobalContext&&) = delete;
  ~GlobalContext();

private:
  GpuWorkCount mainWorkStream;

  vk::DynamicLoader vkDynamicLoader;
  vk::UniqueInstance vkInstance{};
  vk::UniqueDebugUtilsMessengerEXT vkDebugCallback{};
  vk::PhysicalDevice vkPhysDevice{};
  vk::UniqueDevice vkDevice{};

  // We use a single queue for all purposes.
  // Async compute/transfer is too complicated for demos.
  vk::Queue universalQueue{};
  uint32_t universalQueueFamilyIdx{};

  std::unique_ptr<VmaAllocator_T, void (*)(VmaAllocator)> vmaAllocator{nullptr, nullptr};

  std::unique_ptr<DescriptorSetLayoutCache> descriptorSetLayouts;
  std::unique_ptr<ShaderProgramManager> shaderPrograms;
  std::unique_ptr<PipelineManager> pipelineManager;
  std::unique_ptr<DynamicDescriptorPool> descriptorPool;
  std::unique_ptr<ResourceStates> resourceTracking;
  std::unique_ptr<void, void (*)(void*)> tracyCtx;
};

GlobalContext& get_context();

} // namespace etna

#endif // ETNA_GLOBAL_CONTEXT_HPP_INCLUDED
