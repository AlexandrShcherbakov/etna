#include <etna/PipelineBase.hpp>

#include <utility>

#include <etna/PipelineManager.hpp>
#include <etna/ShaderProgram.hpp>


namespace etna
{

PipelineBase::PipelineBase(
  PipelineManager* in_owner, PipelineId in_id, ShaderProgramId in_shader_program_id)
  : owner{in_owner}
  , id{in_id}
  , shaderProgramId{in_shader_program_id}
{
}

PipelineBase::PipelineBase(PipelineBase&& other) noexcept
  : owner{other.owner}
  , id{std::exchange(other.id, PipelineId::Invalid)}
  , shaderProgramId{std::exchange(other.shaderProgramId, ShaderProgramId::Invalid)}
{
}

PipelineBase& PipelineBase::operator=(PipelineBase&& other) noexcept
{
  if (&other == this)
    return *this;

  if (owner != nullptr)
    owner->destroyPipeline(id);
  owner = other.owner;
  id = std::exchange(other.id, PipelineId::Invalid);
  shaderProgramId = std::exchange(other.shaderProgramId, ShaderProgramId::Invalid);

  return *this;
}

PipelineBase::~PipelineBase()
{
  if (owner != nullptr)
    owner->destroyPipeline(id);
}

vk::Pipeline PipelineBase::getVkPipeline() const
{
  return owner->getVkPipeline(id);
}

vk::PipelineLayout PipelineBase::getVkPipelineLayout() const
{
  return owner->getVkPipelineLayout(shaderProgramId);
}

} // namespace etna
