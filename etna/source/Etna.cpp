#include <etna/Etna.hpp>

#include <memory>
#include <vulkan/vulkan_format_traits.hpp>

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include "StateTracking.hpp"


namespace etna
{

static std::unique_ptr<GlobalContext> gContext{};

GlobalContext& get_context()
{
  return *gContext;
}

bool is_initilized()
{
  return static_cast<bool>(gContext);
}

void initialize(const InitParams& params)
{
  gContext.reset(new GlobalContext(params));
}

void shutdown()
{
  gContext->getDescriptorSetLayouts().clear(gContext->getDevice());
  gContext.reset(nullptr);
}

ShaderProgramId create_program(
  const std::string& name, const std::vector<std::string>& shaders_path)
{
  return gContext->getShaderManager().loadProgram(name, shaders_path);
}

void reload_shaders()
{
  gContext->getDescriptorSetLayouts().clear(gContext->getDevice());
  gContext->getShaderManager().reloadPrograms();
  gContext->getPipelineManager().recreate();
  gContext->getDescriptorPool().destroyAllocatedSets();
}

ShaderProgramInfo get_shader_program(ShaderProgramId id)
{
  return gContext->getShaderManager().getProgramInfo(id);
}

ShaderProgramInfo get_shader_program(const std::string& name)
{
  return gContext->getShaderManager().getProgramInfo(name);
}

DescriptorSet create_descriptor_set(
  DescriptorLayoutId layout, vk::CommandBuffer command_buffer, std::vector<Binding> bindings)
{
  auto set = gContext->getDescriptorPool().allocateSet(layout, bindings, command_buffer);
  write_set(set);
  return set;
}

Image create_image_from_bytes(
  Image::CreateInfo info, vk::CommandBuffer command_buffer, const void* data)
{
  const auto blockSize = vk::blockSize(info.format);
  const auto imageSize = blockSize * info.extent.width * info.extent.height * info.extent.depth;
  etna::Buffer stagingBuf = gContext->createBuffer(etna::Buffer::CreateInfo{
    .size = imageSize,
    .bufferUsage = vk::BufferUsageFlagBits::eTransferSrc,
    .memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
    .name = "tmp_staging_buf",
  });

  auto* mappedMem = stagingBuf.map();
  memcpy(mappedMem, data, imageSize);
  stagingBuf.unmap();

  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &beginInfo);

  info.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
  auto image = gContext->createImage(info);
  etna::set_state(
    command_buffer,
    image.get(),
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eTransferDstOptimal,
    image.getAspectMaskByFormat());
  etna::flush_barriers(command_buffer);

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = (VkImageAspectFlags)image.getAspectMaskByFormat();
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = static_cast<uint32_t>(info.layers);

  region.imageOffset = {0, 0, 0};
  region.imageExtent = (VkExtent3D)info.extent;

  vkCmdCopyBufferToImage(
    command_buffer,
    stagingBuf.get(),
    image.get(),
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    1,
    &region);

  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submitInfo{};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = (VkCommandBuffer*)&command_buffer;

  vkQueueSubmit(gContext->getQueue(), 1, &submitInfo, VK_NULL_HANDLE);
  vkQueueWaitIdle(gContext->getQueue());

  stagingBuf.reset();

  return image;
}

void begin_frame()
{
  // TODO: this is brittle. Maybe GpuWorkCount should have frame start calllbacks?
  gContext->getDescriptorPool().beginFrame();
}

void end_frame()
{
  gContext->getMainWorkCount().submit();
}

void set_state(
  vk::CommandBuffer com_buffer,
  vk::Image image,
  vk::PipelineStageFlagBits2 pipeline_stage_flag,
  vk::AccessFlags2 access_flags,
  vk::ImageLayout layout,
  vk::ImageAspectFlags aspect_flags)
{
  etna::get_context().getResourceTracker().setTextureState(
    com_buffer, image, pipeline_stage_flag, access_flags, layout, aspect_flags);
}

void finish_frame(vk::CommandBuffer com_buffer)
{
  etna::get_context().getResourceTracker().flushBarriers(com_buffer);
}

void flush_barriers(vk::CommandBuffer com_buffer)
{
  etna::get_context().getResourceTracker().flushBarriers(com_buffer);
}

} // namespace etna
