#pragma once
#ifndef ETNA_GPU_WORK_COUNT_HPP_INCLUDED
#define ETNA_GPU_WORK_COUNT_HPP_INCLUDED

#include <cstdint>
#include <etna/EtnaConfig.hpp>
#include <etna/Assert.hpp>


namespace etna
{

/**
 * This class represents a continuous "stream" of GPU work. As GPU work is
 * performed concurrently with CPU work, we can only re-use resources shared
 * between the CPU and the GPU after a grace period of a certain amount of
 * frames. Equivalently, we need grace_period + 1 "copies" of every shared
 * resource to avoid race conditions between the CPU and the GPU.
 * By definition, inflight_batches = grace_period + 1, so inflight_batches = 1
 * means that GPU work is done sequentially with CPU work and shared
 * resources can be reused immediately.
 */
class GpuWorkCount
{
public:
  explicit GpuWorkCount(std::size_t inflight_batches)
    : frameNo{0}
    , currentResourceIndex{0}
    , inflightBatches{inflight_batches}
  {
    ETNA_ASSERT(0 < inflight_batches && inflight_batches <= MAX_FRAMES_INFLIGHT);
  }

  /// Get a monotonically increasing index of the current batch of work
  std::uint64_t batchIndex() const { return frameNo; }

  /// Index to use for multi-buffered resources for the current batch
  std::size_t currentResource() const { return currentResourceIndex; }

  /**
   * Get the amount of copies of a shared resource needed to be used with this work stream.
   * Guaranteed to be less than etna::MAX_FRAMES_INFLIGHT, so using std::array is recommended.
   */
  std::size_t multiBufferingCount() const { return inflightBatches; }

  /// Marks the current batch of work as submitted
  void submit()
  {
    ++frameNo;
    currentResourceIndex =
      static_cast<std::size_t>(frameNo % static_cast<std::uint64_t>(inflightBatches));
  }

private:
  std::uint64_t frameNo;
  std::size_t currentResourceIndex;
  const std::size_t inflightBatches;
};

} // namespace etna

#endif // ETNA_GPU_WORK_COUNT_HPP_INCLUDED
