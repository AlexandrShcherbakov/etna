#include <etna/PipelineBase.hpp>
#include <etna/PipelineManager.hpp>

#include <utility>


namespace etna
{

PipelineBase::PipelineBase(PipelineManager* inOwner, std::size_t inId)
  : owner{inOwner}
  , id{inId}
{
}

PipelineBase::PipelineBase(PipelineBase&& other) noexcept
  : owner{other.owner}
  , id{std::exchange(other.id, INVALID_PIPELINE_ID)}
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

}
