#include "etna/DebugUtils.hpp"

#include "etna/GlobalContext.hpp"

namespace etna
{

  void set_debug_name_base(uint64_t object, vk::DebugReportObjectTypeEXT object_type, const char *name)
  {
    vk::DebugMarkerObjectNameInfoEXT debugNameInfo = {
      .objectType = object_type,
      .object = object,
      .pObjectName = name
    };
    etna::get_context().getDevice().debugMarkerSetObjectNameEXT(&debugNameInfo);
  }

  void set_debug_name(vk::Image image, const char *name)
  {
    set_debug_name_base((uint64_t)(VkImage)image, vk::DebugReportObjectTypeEXT::eImage, name);
  }

  void set_debug_name(vk::Buffer buffer, const char *name)
  {
    set_debug_name_base((uint64_t)(VkBuffer)buffer, vk::DebugReportObjectTypeEXT::eBuffer, name);
  }

  void set_debug_name(vk::Sampler sampler, const char *name)
  {
    set_debug_name_base((uint64_t)(VkSampler)sampler, vk::DebugReportObjectTypeEXT::eSampler, name);
  }

}
