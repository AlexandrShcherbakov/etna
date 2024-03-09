#include <etna/PipelineBase.hpp>

#include <utility>

#include <etna/PipelineManager.hpp>
#include <etna/ShaderProgram.hpp>


namespace etna
{

PipelineBase::PipelineBase(
  PipelineManager* inOwner, PipelineId inId, ShaderProgramId inShaderProgramId)
  : owner{inOwner}
  , id{inId}
  , shaderProgramId{inShaderProgramId}
{
}

PipelineBase::PipelineBase(PipelineBase&& other) noexcept
  : owner{other.owner}
  , id{std::exchange(other.id, INVALID_PIPELINE_ID)}
  , shaderProgramId{std::exchange(other.shaderProgramId, INVALID_SHADER_PROGRAM_ID)}
{
}

PipelineBase& PipelineBase::operator=(PipelineBase&& other) noexcept
{
  if (&other == this)
    return *this;

  if (owner != nullptr)
    owner->destroyPipeline(id);
  owner = other.owner;
  id = std::exchange(other.id, INVALID_PIPELINE_ID);
  shaderProgramId = std::exchange(other.shaderProgramId, INVALID_SHADER_PROGRAM_ID);

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
