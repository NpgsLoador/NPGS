#include "stdafx.h"
#include "ConfigWatcher.hpp"

#include <cstddef>
#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <utility>

#include "Engine/Core/Logger.hpp"

namespace Npgs
{
    namespace
    {
        std::string WideCharToUnicode8(const std::wstring& Str)
        {
            if (Str.empty())
            {
                return {};
            }

            int SizeNeeded = WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), static_cast<int>(Str.size()),
                                                 nullptr, 0, nullptr, nullptr);
            std::string Target(SizeNeeded, '\0');
            WideCharToMultiByte(CP_UTF8, 0, Str.c_str(), static_cast<int>(Str.size()),
                                Target.data(), SizeNeeded, nullptr, nullptr);
            return Target;
        }
    }

    FConfigWatcher::FConfigWatcher(std::function<void(const nlohmann::json&)> UpdateCallback)
        : UpdateCallback_(std::move(UpdateCallback))
        , WatchThread_(&FConfigWatcher::WatchLoop, this)
    {
        if (!FindConfigPath())
        {
            NpgsCoreError("Failed to find config.json.");
            throw std::runtime_error("Failed to find config.json");
        }

        AppName_ = GetAppName();
        InitialLoop();
    }

    FConfigWatcher::~FConfigWatcher()
    {
        bStopWatchThread_ = true;
        if (WatchHandle_ != INVALID_HANDLE_VALUE)
        {
            CancelIoEx(WatchHandle_, nullptr);
            CloseHandle(WatchHandle_);
        }
    }

    void FConfigWatcher::InitialLoop() const
    {
        std::ifstream ConfigFile(ConfigFilename_);
        if (!ConfigFile.is_open())
        {
            NpgsCoreError(R"(Failed to open config file "{}".)", WideCharToUnicode8(ConfigFilename_));
            return;
        }

        try
        {
            nlohmann::json Data = nlohmann::json::parse(ConfigFile);
            if (Data.contains("wproperties") && Data["wproperties"].contains(AppName_))
            {
                nlohmann::json MonitorProperties = Data["wproperties"][AppName_];
                if (UpdateCallback_)
                {
                    UpdateCallback_(MonitorProperties);
                }
            }
        }
        catch (const nlohmann::json::parse_error& e)
        {
            NpgsCoreError("Json parse error on initial load: ", e.what());
            return;
        }
    }

    void FConfigWatcher::WatchLoop()
    {
        std::filesystem::path ConfigPath(ConfigFilename_);
        std::wstring Directory = ConfigPath.parent_path().wstring();
        std::wstring Filename  = ConfigPath.filename().wstring();

        WatchHandle_ = CreateFile(
            Directory.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);

        if (WatchHandle_ == INVALID_HANDLE_VALUE)
        {
            throw std::runtime_error("Failed to create watch handle.");
        }

        std::array<std::byte, 4096> Buffer{};
        DWORD      BytesReturned{};
        OVERLAPPED Overlapped{};
        Overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

        while (!bStopWatchThread_)
        {
            if (ReadDirectoryChangesW(WatchHandle_, Buffer.data(), static_cast<DWORD>(Buffer.size()), FALSE,
                                      FILE_NOTIFY_CHANGE_LAST_WRITE, &BytesReturned, &Overlapped, nullptr))
            {
                GetOverlappedResult(WatchHandle_, &Overlapped, &BytesReturned, TRUE);
                if (Overlapped.hEvent != nullptr)
                {
                    ResetEvent(Overlapped.hEvent);
                }

                FILE_NOTIFY_INFORMATION* Notify = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(Buffer.data());
                std::wstring ChangedFile(Notify->FileName, Notify->FileNameLength / sizeof(WCHAR));

                if (ChangedFile == Filename)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    InitialLoop();
                }
            }
            else
            {
                if (GetLastError() != ERROR_IO_PENDING)
                {
                    break;
                }
            }
        }

        if (Overlapped.hEvent != nullptr)
        {
            CloseHandle(Overlapped.hEvent);
        }
    }

    bool FConfigWatcher::FindConfigPath()
    {
        std::array<wchar_t, MAX_PATH> ExePathW{};
        GetModuleFileName(nullptr, ExePathW.data(), MAX_PATH);
        std::filesystem::path CurrentPath(ExePathW.data());

        // Navigate up from ".../workshop/content/431960/..."
        for (int i = 0; i != 4; ++i)
        {
            if (CurrentPath.has_parent_path())
            {
                CurrentPath = CurrentPath.parent_path();
            }
            else
            {
                return false;
            }
        }

        CurrentPath = CurrentPath / "common" / "wallpaper_engine" / "config.json";
        if (std::filesystem::exists(CurrentPath))
        {
            ConfigFilename_ = CurrentPath.wstring();
            return true;
        }

        NpgsCoreError("Test config.json not found");

        return false;
    }

    std::string FConfigWatcher::GetAppName()
    {
        std::array<wchar_t, MAX_PATH> ExePathW{};
        GetModuleFileNameW(nullptr, ExePathW.data(), MAX_PATH);
        std::wstring WideChar(ExePathW.data());
        std::string Key = WideCharToUnicode8(WideChar);
        std::ranges::replace(Key, '\\', '/');
        return Key;
    }
} // namespace Npgs
