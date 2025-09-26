#include <cmath>
#include <cstring>
#include <limits>

#include "Engine/Core/Base/Base.hpp"

namespace Npgs
{
#ifdef NPGS_ENABLE_ENUM_BIT_OPERATOR
    template <typename EnumType>
    constexpr EnumType operator&(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) &
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }

    template <typename EnumType>
    constexpr EnumType operator|(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) |
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }

    template <typename EnumType>
    constexpr EnumType operator^(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) ^
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }

    template <typename EnumType>
    constexpr EnumType operator~(EnumType Value)
    {
        return static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(Value));
    }

    template <typename EnumType>
    constexpr EnumType operator&=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) &=
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }

    template <typename EnumType>
    constexpr EnumType operator|=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) |=
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }

    template <typename EnumType>
    constexpr EnumType operator^=(EnumType Lhs, EnumType Rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(Lhs) ^=
                                     static_cast<std::underlying_type_t<EnumType>>(Rhs));
    }
#endif // NPGS_ENABLE_ENUM_BIT_OPERATOR
} // namespace Npgs

namespace Npgs::Util
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
} // namespace Npgs::Util
