#pragma once
#ifndef ETNA_BLOCKING_TRANSFER_HELPER_HPP_INCLUDED
#define ETNA_BLOCKING_TRANSFER_HELPER_HPP_INCLUDED

#include <type_traits>

#include <etna/Vulkan.hpp>
#include <etna/Buffer.hpp>
#include <etna/OneShotCmdMgr.hpp>


namespace etna
{

/**
 * Simplest possible GPU-CPU data transfer helper:
 * blocks CPU until the transfer operation finishes.
 * WARNING: Should never be used inside the main loop of
 * an interactive application! Only appropriate for initial
 * "bulk" uploading of scene data!
 */
class BlockingTransferHelper
{
public:
  struct CreateInfo
  {
    vk::DeviceSize stagingSize;
  };

  explicit BlockingTransferHelper(CreateInfo info);

  BlockingTransferHelper(const BlockingTransferHelper&) = delete;
  BlockingTransferHelper& operator=(const BlockingTransferHelper&) = delete;
  BlockingTransferHelper(BlockingTransferHelper&&) = delete;
  BlockingTransferHelper& operator=(BlockingTransferHelper&&) = delete;

  template <class T>
    requires std::is_trivially_copyable_v<T>
  void uploadBuffer(
    OneShotCmdMgr& cmd_mgr, Buffer& dst, std::uint32_t offset, std::span<T const> src)
  {
    std::span<std::byte const> raw{
      reinterpret_cast<const std::byte*>(src.data()), src.size_bytes()};
    uploadBuffer(cmd_mgr, dst, offset, raw);
  }

  void uploadBuffer(
    OneShotCmdMgr& cmd_mgr, Buffer& dst, std::uint32_t offset, std::span<std::byte const> src);



  template <class T>
    requires std::is_trivially_copyable_v<T>
  void readbackBuffer(
    OneShotCmdMgr& cmd_mgr, std::span<T> dst, const Buffer& src, std::uint32_t offset)
  {
    std::span<std::byte> raw{reinterpret_cast<std::byte*>(dst.data()), dst.size_bytes()};
    readbackBuffer(cmd_mgr, raw, src, offset);
  }

  void readbackBuffer(
    OneShotCmdMgr& cmd_mgr, std::span<std::byte> dst, const Buffer& src, std::uint32_t offset);


  // NOTE: uploads only mip 0 and layer 0 for now and doesn't support 3D images
  void uploadImage(
    OneShotCmdMgr& cmd_mgr, Image& dst, uint32_t mip_level, uint32_t layer, std::span<std::byte const> src);

private:
  vk::DeviceSize stagingSize;
  Buffer stagingBuffer;
};

} // namespace etna

#endif // ETNA_BLOCKING_TRANSFER_HELPER_HPP_INCLUDED
