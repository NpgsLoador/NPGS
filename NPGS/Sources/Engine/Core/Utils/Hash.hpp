#pragma once

#include <cstddef>
#include <concepts>
#include <string>
#include <string_view>
#include <unordered_map>

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
    using FStringHeteroHashTable = std::unordered_map<Key, Value, FStringViewHeteroHash, FStringViewHeteroEqual>;

    template <typename Ty>
    NPGS_INLINE void HashCombine(const Ty& Value, std::size_t& Seed)
    {
        std::hash<Ty> Hasher;
        Seed ^= Hasher(Value) + 0x9e3779b9 + (Seed << 6) + (Seed >> 2);
    }
} // namespace Npgs::Utils
