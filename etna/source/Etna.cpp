#include <memory>

#include "StateTracking.hpp"

#include <etna/GlobalContext.hpp>
#include <etna/Etna.hpp>


namespace etna
{
  static std::unique_ptr<GlobalContext> g_context {};

  GlobalContext &get_context()
  {
    return *g_context;
  }
  
  bool is_initilized()
  {
    return static_cast<bool>(g_context);
  }
  
  void initialize(const InitParams &params)
  {
    g_context.reset(new GlobalContext(params));
  }
  
  void shutdown()
  {
    g_context->getDescriptorSetLayouts().clear(g_context->getDevice());
    g_context.reset(nullptr);
  }

  ShaderProgramId create_program(const std::string &name, const std::vector<std::string> &shaders_path)
  {
    return g_context->getShaderManager().loadProgram(name, shaders_path);
  }

  void reload_shaders()
  {
    g_context->getDescriptorSetLayouts().clear(g_context->getDevice());
    g_context->getShaderManager().reloadPrograms();
    g_context->getPipelineManager().recreate();
    g_context->getDescriptorPool().destroyAllocatedSets();
  }

  ShaderProgramInfo get_shader_program(ShaderProgramId id)
  {
    return g_context->getShaderManager().getProgramInfo(id);
  }
  
  ShaderProgramInfo get_shader_program(const std::string &name)
  {
    return g_context->getShaderManager().getProgramInfo(name);
  }

  DescriptorSet create_descriptor_set(DescriptorLayoutId layout, const vk::ArrayProxy<const Binding> &bindings)
  {
    auto set = g_context->getDescriptorPool().allocateSet(layout);
    write_set(set, bindings);
    return set;
  }

  /*Todo: submit logic here*/
  void submit()
  {
    g_context->getDescriptorPool().flip();
  }

  void set_state(VkCommandBuffer com_buffer, vk::Image image,
    vk::PipelineStageFlagBits2 pipeline_stage_flag, vk::AccessFlags2 access_flags,
    vk::ImageLayout layout, vk::ImageAspectFlags aspect_flags)
  {
    etna::get_context().getResourceTracker().setTextureState(com_buffer, image,
      pipeline_stage_flag, access_flags, layout, aspect_flags);
  }

  void finish_frame(VkCommandBuffer com_buffer)
  {
    etna::get_context().getResourceTracker().flushBarriers(com_buffer);
  }
}
