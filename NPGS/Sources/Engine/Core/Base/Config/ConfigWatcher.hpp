#pragma once

#include <functional>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>
#include <Windows.h>

namespace Npgs
{
    class FConfigWatcher
    {
    public:
        FConfigWatcher(std::function<void(const nlohmann::json&)> UpdateCallback);
        FConfigWatcher(const FConfigWatcher&) = delete;
        FConfigWatcher(FConfigWatcher&&)      = delete;
        ~FConfigWatcher();

        FConfigWatcher& operator=(const FConfigWatcher&) = delete;
        FConfigWatcher& operator=(FConfigWatcher&&)      = delete;

    private:
        void InitialLoop() const;
        void WatchLoop();
        bool FindConfigPath();
        std::string GetAppName();

        std::wstring                               ConfigFilename_;
        std::string                                AppName_;
        std::function<void(const nlohmann::json&)> UpdateCallback_;
        std::jthread                               WatchThread_;
        HANDLE                                     WatchHandle_{ INVALID_HANDLE_VALUE };
        bool                                       bStopWatchThread_{ false };
    };
} // namespace Npgs
