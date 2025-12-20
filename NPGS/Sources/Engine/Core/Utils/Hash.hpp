#pragma once

#include <cstddef>
#include <concepts>
#include <span>
#include <string>
#include <string_view>

#include <ankerl/unordered_dense.h>
#include "Engine/Core/Base/Base.hpp"

namespace Npgs::Utils
{
    struct FStringViewHeteroEqual
    {
        using is_transparent = void;

        bool operator()(std::string_view Lhs, std::string_view Rhs) const
        {
            return Lhs == Rhs;
        }

        bool operator()(std::string_view Lhs, const std::string& Rhs) const
        {
            return Lhs == Rhs;
        }

        bool operator()(const std::string& Lhs, std::string_view Rhs) const
        {
            return Lhs == Rhs;
        }

        bool operator()(const std::string& Lhs, const std::string& Rhs) const
        {
            return Lhs == Rhs;
        }

        bool operator()(std::string_view Lhs, const char* Rhs) const
        {
            return Lhs == Rhs;
        }

        bool operator()(const char* Lhs, std::string_view Rhs) const
        {
            return Lhs == Rhs;
        }
    };

    struct FStringViewHeteroHash
    {
        using is_transparent = void;

        std::size_t operator()(std::string_view Key) const
        {
            return std::hash<std::string_view>()(Key);
        }
    };

    template <typename Key, typename Value>
    requires std::same_as<Key, std::string>
    using TStringHeteroHashTable = ankerl::unordered_dense::map<Key, Value, FStringViewHeteroHash, FStringViewHeteroEqual>;

    template <typename Ty, typename Hash = std::hash<Ty>>
    NPGS_INLINE void HashCombine(const Ty& Value, std::size_t& Seed)
    {
        Hash Hasher{};
        Seed ^= Hasher(Value) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
    }

    template <typename Ty, typename Hash = std::hash<Ty>>
    NPGS_INLINE void HashCombineRange(std::span<const Ty> Values, std::size_t& Seed)
    {
        for (const auto& Value : Values)
        {
            HashCombine<Ty, Hash>(Value, Seed);
        }
    }
} // namespace Npgs::Utils
