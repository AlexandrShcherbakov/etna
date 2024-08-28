#include <etna/Etna.hpp>

#include <memory>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_format_traits.hpp>

#include <etna/GlobalContext.hpp>
#include <etna/PipelineManager.hpp>
#include <vulkan/vulkan_structs.hpp>
#include "StateTracking.hpp"
#include "etna/Vulkan.hpp"


namespace etna
{

static std::unique_ptr<GlobalContext> gContext{};

GlobalContext& get_context()
{
  ETNA_VERIFYF(gContext, "Tried to use Etna before initializing it!");
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
  const char* name, std::initializer_list<std::filesystem::path> shaders_path)
{
  return gContext->getShaderManager().loadProgram(name, shaders_path);
}

ShaderProgramId get_program_id(const char* name)
{
  return gContext->getShaderManager().tryGetProgram(name);
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

ShaderProgramInfo get_shader_program(const char* name)
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

Image create_image_from_bytes(Image::CreateInfo info, vk::CommandBuffer cmd_buf, const void* data)
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
  std::memcpy(mappedMem, data, imageSize);
  stagingBuf.unmap();

  ETNA_CHECK_VK_RESULT(cmd_buf.begin(vk::CommandBufferBeginInfo{
    .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
  }));

  info.imageUsage |= vk::ImageUsageFlagBits::eTransferDst;
  auto image = gContext->createImage(info);
  etna::set_state(
    cmd_buf,
    image.get(),
    vk::PipelineStageFlagBits2::eTransfer,
    vk::AccessFlagBits2::eTransferWrite,
    vk::ImageLayout::eTransferDstOptimal,
    image.getAspectMaskByFormat());
  etna::flush_barriers(cmd_buf);

  vk::BufferImageCopy region{
    .bufferOffset = 0,
    .bufferRowLength = 0,
    .bufferImageHeight = 0,
    .imageSubresource =
      vk::ImageSubresourceLayers{
        .aspectMask = image.getAspectMaskByFormat(),
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = static_cast<std::uint32_t>(info.layers),
      },
    .imageOffset = {0, 0, 0},
    .imageExtent = info.extent,
  };

  cmd_buf.copyBufferToImage(
    stagingBuf.get(), image.get(), vk::ImageLayout::eTransferDstOptimal, 1, &region);

  ETNA_CHECK_VK_RESULT(cmd_buf.end());

  vk::SubmitInfo submitInfo{
    .commandBufferCount = 1,
    .pCommandBuffers = &cmd_buf,
  };

  ETNA_CHECK_VK_RESULT(gContext->getQueue().submit(1, &submitInfo, {}));
  ETNA_CHECK_VK_RESULT(gContext->getQueue().waitIdle());

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
