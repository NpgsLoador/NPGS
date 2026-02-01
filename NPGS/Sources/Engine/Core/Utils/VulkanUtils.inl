#include "VulkanUtils.hpp"
#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Utils
{
    NPGS_INLINE constexpr bool IsSpecialLayout(vk::ImageLayout Layout)
    {
        return Layout == vk::ImageLayout::eUndefined      ||
               Layout == vk::ImageLayout::ePreinitialized ||
               Layout == vk::ImageLayout::ePresentSrcKHR  ||
               Layout == vk::ImageLayout::eSharedPresentKHR;
    }
} // namespace Npgs::Utils
