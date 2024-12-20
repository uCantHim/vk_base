#pragma once

#include <vulkan/vulkan.hpp>

namespace trc
{
    // To counter a recent API break in vulkan.hpp (they move the loader
    // declarations from ::vk to ::vk::detail):
#if (VK_HEADER_VERSION < 303)
    using VulkanDispatchLoaderDynamic = vk::DispatchLoaderDynamic;
    using VulkanDispatchLoaderStatic = vk::DispatchLoaderStatic;
#else
    using VulkanDispatchLoaderDynamic = vk::detail::DispatchLoaderDynamic;
    using VulkanDispatchLoaderStatic = vk::detail::DispatchLoaderStatic;
#endif
} // namespace trc
