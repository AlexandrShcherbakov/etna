#include "DebugUtils.hpp"

#include <bit>
#include <type_traits>

#include "etna/GlobalContext.hpp"


namespace etna
{

#ifdef ETNA_SET_VULKAN_DEBUG_NAMES
template <class T>
static void set_debug_name_base(
  T object, vk::DebugReportObjectTypeEXT object_type, const char* name)
{
  const auto nativeHandle = static_cast<typename std::remove_cvref_t<T>::NativeType>(object);
  // The NativeType for a vulkan object can either be a uint64_t,
  // or an opaque pointer, hence we need a bit case.
  vk::DebugMarkerObjectNameInfoEXT debugNameInfo = {
    .objectType = object_type,
    .object = std::bit_cast<uint64_t>(nativeHandle),
    .pObjectName = name,
  };
  auto retcode = etna::get_context().getDevice().debugMarkerSetObjectNameEXT(&debugNameInfo);
  ETNA_ASSERTF(
    retcode == vk::Result::eSuccess,
    "Error {} occurred while trying to set a debug name!",
    vk::to_string(retcode));
}
#else
template <class T>
static void set_debug_name_base(T, vk::DebugReportObjectTypeEXT, const char*)
{
}
#endif

void set_debug_name(vk::Image image, const char* name)
{
  set_debug_name_base(image, vk::DebugReportObjectTypeEXT::eImage, name);
}

void set_debug_name(vk::Buffer buffer, const char* name)
{
  set_debug_name_base(buffer, vk::DebugReportObjectTypeEXT::eBuffer, name);
}

void set_debug_name(vk::Sampler sampler, const char* name)
{
  set_debug_name_base(sampler, vk::DebugReportObjectTypeEXT::eSampler, name);
}

} // namespace etna
