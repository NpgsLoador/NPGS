#include "stdafx.h"
#include "FileLoader.hpp"

#include <utility>
#include "Engine/Core/Logger.hpp"

namespace Npgs
{
    FFileLoader::FFileLoader(std::string_view Filename)
    {
        Load(Filename);
    }

    FFileLoader::FFileLoader(FFileLoader&& Other) noexcept
        : Data_(std::exchange(Other.Data_, nullptr))
        , Size_(std::exchange(Other.Size_, 0))
        , Mode_(std::exchange(Other.Mode_, EMode::kNone))
    {
    }

    FFileLoader::~FFileLoader()
    {
        Unload();
    }

    FFileLoader& FFileLoader::operator=(FFileLoader&& Other) noexcept
    {
        if (this != &Other)
        {
            Unload();
            Data_ = std::exchange(Other.Data_, nullptr);
            Size_ = std::exchange(Other.Size_, 0);
            Mode_ = std::exchange(Other.Mode_, EMode::kNone);
        }

        return *this;
    }

    bool FFileLoader::Load(std::string_view Filename)
    {
        Unload();

        HANDLE File = CreateFileA(Filename.data(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
        if (File == INVALID_HANDLE_VALUE)
        {
            NpgsCoreError("Fatal error: Failed to load file \"{}\": No such file or directory.", Filename);
            return false;
        }

        LARGE_INTEGER FileSize{};
        if (!GetFileSizeEx(File, &FileSize))
        {
            NpgsCoreError("Fatal error: Failed to get FileSize of \"{}\".", Filename);
            return false;
        }

        Size_ = static_cast<std::size_t>(FileSize.QuadPart);
        bool bResult = false;

        constexpr std::size_t kMmapThreshold = 16uz * 1024 * 1024;
        if (Size_ > kMmapThreshold)
        {
            bResult = LoadMmapFileInternal(File);
        }
        else
        {
            bResult = LoadReadFileInternal(File);
        }

        CloseHandle(File);

        if (!bResult)
        {
            Unload();
        }

        return bResult;
    }

    void FFileLoader::Unload()
    {
        if (Data_ != nullptr)
        {
            if (Mode_ == EMode::kHeap)
            {
                VirtualFree(Data_, 0, MEM_RELEASE);
            }
            else if (Mode_ == EMode::kMmap)
            {
                UnmapViewOfFile(Data_);
            }

            Data_ = nullptr;
        }

        Size_ = 0;
        Mode_ = EMode::kNone;
    }

    bool FFileLoader::LoadMmapFileInternal(HANDLE File)
    {
        HANDLE Mmap = CreateFileMappingA(File, nullptr, PAGE_READONLY, 0, 0, nullptr);
        if (!Mmap)
        {
            return false;
        }

        void* MapView = MapViewOfFile(Mmap, FILE_MAP_READ, 0, 0, 0);
        CloseHandle(File);

        if (MapView == nullptr)
        {
            return false;
        }

        Data_ = MapView;
        Mode_ = EMode::kMmap;
        return true;
    }

    bool FFileLoader::LoadReadFileInternal(HANDLE File)
    {
        void* Buffer = VirtualAlloc(nullptr, Size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (Buffer == nullptr)
        {
            return false;
        }

        DWORD BytesRead = 0;
        if (!ReadFile(File, Buffer, static_cast<DWORD>(Size_), &BytesRead, nullptr) || BytesRead != Size_)
        {
            VirtualFree(Buffer, 0, MEM_RELEASE);
            return false;
        }

        Data_ = Buffer;
        Mode_ = EMode::kHeap;
        return true;
    }
} // namespace Npgs
