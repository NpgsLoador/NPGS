#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>
#include <Windows.h>

namespace Npgs
{
    class FFileLoader
    {
    private:
        enum class EMode
        {
            kNone, kHeap, kMmap
        };

    public:
        FFileLoader() = default;
        FFileLoader(std::string_view Filename);
        FFileLoader(const FFileLoader&) = delete;
        FFileLoader(FFileLoader&& Other) noexcept;
        ~FFileLoader();

        FFileLoader& operator=(const FFileLoader&) = delete;
        FFileLoader& operator=(FFileLoader&& Other) noexcept;

        bool Load(std::string_view Filename);
        void Unload();

        template <typename Ty>
        std::vector<Ty> StripData();

        template <typename Ty>
        requires std::is_standard_layout_v<Ty> && std::is_trivial_v<Ty>
        std::span<const Ty> GetDataAs() const;
        
        bool Empty() const;
        std::size_t Size() const;

    private:
        bool LoadMmapFileInternal(HANDLE File);
        bool LoadReadFileInternal(HANDLE File);

    private:
        void*       Data_{ nullptr };
        std::size_t Size_{};
        EMode       Mode_{ EMode::kNone };
    };
} // namespace Npgs

#include "FileLoader.inl"
