#pragma once

#include <vulkan/vulkan.hpp>

// Validation macros
// -----------------
#define VulkanCheck(Target) if (VkResult Result = Target; Result != VK_SUCCESS) return static_cast<vk::Result>(Result);

#define VulkanCheckWithMessage(Target, Message)                                       \
if (VkResult Result = Target; Result != VK_SUCCESS)                                   \
{                                                                                     \
    NpgsCoreError("{}: {}", Message, vk::to_string(static_cast<vk::Result>(Result))); \
    return static_cast<vk::Result>(Result);                                           \
}

#define VulkanHppCheck(Target) if (vk::Result Result = Target; Result != vk::Result::eSuccess) return Result;

#define VulkanHppCheckWithMessage(Target, Message)              \
if (vk::Result Result = Target; Result != vk::Result::eSuccess) \
{                                                               \
    NpgsCoreError("{}: {}", Message, vk::to_string(Result));    \
    return Result;                                              \
}

namespace Npgs::Utils
{
    constexpr bool IsSpecialLayout(vk::ImageLayout Layout);
} // namespace Npgs::Utils

#include "VulkanUtils.inl"
