#include "DebugUtils.hpp"

#include <bit>
#include <type_traits>

#include "etna/GlobalContext.hpp"


namespace etna
{

// NOTE: Previously we used VK_EXT_debug_marker extension,
// but it is considered to be abandoned in favour of VK_EXT_debug_utils.
#ifdef ETNA_SET_VULKAN_DEBUG_NAMES
template <class T>
static void set_debug_name_base(T object, vk::ObjectType object_type, const char* name)
{
  const auto nativeHandle = static_cast<typename std::remove_cvref_t<T>::NativeType>(object);
  // The NativeType for a vulkan object can either be a uint64_t,
  // or an opaque pointer, hence we need a bit case.
  vk::DebugUtilsObjectNameInfoEXT debugNameInfo = {
    .objectType = object_type,
    .objectHandle = std::bit_cast<uint64_t>(nativeHandle),
    .pObjectName = name,
  };
  auto retcode = etna::get_context().getDevice().setDebugUtilsObjectNameEXT(&debugNameInfo);
  ETNA_VERIFYF(
    retcode == vk::Result::eSuccess,
    "Error {} occurred while trying to set a debug name!",
    vk::to_string(retcode));
}
#else
template <class T>
static void set_debug_name_base(T, vk::ObjectType, const char*)
{
}
#endif

void set_debug_name(vk::Image image, const char* name)
{
  set_debug_name_base(image, vk::ObjectType::eImage, name);
}

void set_debug_name(vk::ImageView image_view, const char* name)
{
  set_debug_name_base(image_view, vk::ObjectType::eImageView, name);
}

void set_debug_name(vk::Buffer buffer, const char* name)
{
  set_debug_name_base(buffer, vk::ObjectType::eBuffer, name);
}

void set_debug_name(vk::Sampler sampler, const char* name)
{
  set_debug_name_base(sampler, vk::ObjectType::eSampler, name);
}

} // namespace etna
