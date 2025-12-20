#include "Utils.hpp"

#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
#ifdef NPGS_ENABLE_ENUM_BIT_OPERATOR
    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator&(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) & std::to_underlying(Rhs));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator|(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) | std::to_underlying(Rhs));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator^(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) ^ std::to_underlying(Rhs));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator~(EnumType Value)
    {
        return static_cast<EnumType>(~std::to_underlying(Value));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator&=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) &= std::to_underlying(Rhs));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator|=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) |= std::to_underlying(Rhs));
    }

    template <typename EnumType>
    NPGS_INLINE constexpr EnumType operator^=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(std::to_underlying(Lhs) ^= std::to_underlying(Rhs));
    }
#endif // NPGS_ENABLE_ENUM_BIT_OPERATOR
} // namespace Npgs

namespace Npgs::Utils
{
    NPGS_INLINE bool Equal(const char* Lhs, const char* Rhs)
    {
        return std::strcmp(Lhs, Rhs) == 0;
    }

    NPGS_INLINE bool Equal(float Lhs, float Rhs)
    {
        return std::abs(Lhs - Rhs) <= std::numeric_limits<float>::epsilon();
    }

    NPGS_INLINE bool Equal(double Lhs, double Rhs)
    {
        return std::abs(Lhs - Rhs) <= std::numeric_limits<double>::epsilon();
    }

    NPGS_INLINE constexpr bool IsSpecialLayout(vk::ImageLayout Layout)
    {
        return Layout == vk::ImageLayout::eUndefined      ||
               Layout == vk::ImageLayout::ePreinitialized ||
               Layout == vk::ImageLayout::ePresentSrcKHR  ||
               Layout == vk::ImageLayout::eSharedPresentKHR;
    }
} // namespace Npgs::Utils
