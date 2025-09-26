#pragma once

namespace Npgs
{
#ifdef NPGS_ENABLE_ENUM_BIT_OPERATOR
    template <typename EnumType>
    constexpr EnumType operator&(EnumType Lhs, EnumType Rhs);

    template <typename EnumType>
    constexpr EnumType operator|(EnumType Lhs, EnumType Rhs);

    template <typename EnumType>
    constexpr EnumType operator^(EnumType Lhs, EnumType Rhs);

    template <typename EnumType>
    constexpr EnumType operator~(EnumType Value);

    template <typename EnumType>
    constexpr EnumType operator&=(EnumType Lhs, EnumType Rhs);

    template <typename EnumType>
    constexpr EnumType operator|=(EnumType Lhs, EnumType Rhs);

    template <typename EnumType>
    constexpr EnumType operator^=(EnumType Lhs, EnumType Rhs);
#endif // NPGS_ENABLE_ENUM_BIT_OPERATOR
} // namespace Npgs

namespace Npgs::Util
{
    bool Equal(const char* Lhs, const char* Rhs);
    bool Equal(float Lhs, float Rhs);
    bool Equal(double Lhs, double Rhs);
} // namespace Npgs::Util

#include "Utils.inl"
