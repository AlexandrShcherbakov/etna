#pragma once
#ifndef ETNA_ONE_SHOT_CMD_MGR_HPP_INCLUDED
#define ETNA_ONE_SHOT_CMD_MGR_HPP_INCLUDED

#include <etna/Vulkan.hpp>


namespace etna
{

/**
 * Provides a single command buffer that can be recorder,
 * submitted and waited on.
 * WARNING: Should never be used inside the main loop of
 * an interactive application!
 */
class OneShotCmdMgr
{
public:
  struct Dependencies
  {
    vk::Device device;

    vk::Queue submitQueue;
    std::uint32_t queueFamily;
  };

  explicit OneShotCmdMgr(const Dependencies& deps);

  OneShotCmdMgr(const OneShotCmdMgr&) = delete;
  OneShotCmdMgr& operator=(const OneShotCmdMgr&) = delete;
  OneShotCmdMgr(OneShotCmdMgr&&) = delete;
  OneShotCmdMgr& operator=(OneShotCmdMgr&&) = delete;

  // Gets the command buffer for some one-shot commands
  vk::CommandBuffer start();

  // Submits the one-shot command buffer previously acquired and waits for completion
  void submitAndWait(vk::CommandBuffer buffer);

private:
  vk::Device device;
  vk::Queue submitQueue;

  vk::UniqueCommandPool pool;
  vk::UniqueCommandBuffer commandBuffer;
  vk::UniqueFence oneShotFinished;
};

} // namespace etna

#endif // ETNA_ONE_SHOT_CMD_MGR_HPP_INCLUDED
