#pragma once

#include <memory>
#include <spdlog/spdlog.h>

namespace Npgs
{
    class FLogger
    {
    public:
        static void Initialize();
        static std::shared_ptr<spdlog::logger>& GetCoreLogger();
        static std::shared_ptr<spdlog::logger>& GetClientLogger();

    private:
        FLogger()  = default;
        ~FLogger() = default;

        static std::shared_ptr<spdlog::logger> CoreLogger_;
        static std::shared_ptr<spdlog::logger> ClientLogger_;
    };
} // namespace Npgs::Utils

#include "Logger.inl"

#if defined(NPGS_ENABLE_CONSOLE_LOGGER)
// Core logger
// -----------
#define NpgsCoreCritical(...) ::Npgs::FLogger::GetCoreLogger()->critical(__VA_ARGS__)
#define NpgsCoreError(...)    ::Npgs::FLogger::GetCoreLogger()->error(__VA_ARGS__)
#define NpgsCoreInfo(...)     ::Npgs::FLogger::GetCoreLogger()->info(__VA_ARGS__)
#define NpgsCoreTrace(...)    ::Npgs::FLogger::GetCoreLogger()->trace(__VA_ARGS__)
#define NpgsCoreWarn(...)     ::Npgs::FLogger::GetCoreLogger()->warn(__VA_ARGS__)

// Client logger
// -------------
#define NpgsCritical(...)     ::Npgs::FLogger::GetClientLogger()->critical(__VA_ARGS__)
#define NpgsError(...)        ::Npgs::FLogger::GetClientLogger()->error(__VA_ARGS__)
#define NpgsInfo(...)         ::Npgs::FLogger::GetClientLogger()->info(__VA_ARGS__)
#define NpgsTrace(...)        ::Npgs::FLogger::GetClientLogger()->trace(__VA_ARGS__)
#define NpgsWarn(...)         ::Npgs::FLogger::GetClientLogger()->warn(__VA_ARGS__)

#elif defined(NPGS_ENABLE_FILE_LOGGER)

#define NpgsCoreCritical(...) ::Npgs::FLogger::GetCoreLogger()->critical(__VA_ARGS__)
#define NpgsCoreError(...)    ::Npgs::FLogger::GetCoreLogger()->error(__VA_ARGS__)
#define NpgsCoreInfo(...)     ::Npgs::FLogger::GetCoreLogger()->info(__VA_ARGS__)
#define NpgsCoreTrace(...)    static_cast<void>(0)
#define NpgsCoreWarn(...)     ::Npgs::FLogger::GetCoreLogger()->warn(__VA_ARGS__)

// Client logger
// -------------
#define NpgsCritical(...)     ::Npgs::FLogger::GetClientLogger()->critical(__VA_ARGS__)
#define NpgsError(...)        ::Npgs::FLogger::GetClientLogger()->error(__VA_ARGS__)
#define NpgsInfo(...)         ::Npgs::FLogger::GetClientLogger()->info(__VA_ARGS__)
#define NpgsTrace(...)        static_cast<void>(0)
#define NpgsWarn(...)         ::Npgs::FLogger::GetClientLogger()->warn(__VA_ARGS__)

#else

#define NpgsCoreCritical(...) static_cast<void>(0)
#define NpgsCoreError(...)    static_cast<void>(0)
#define NpgsCoreInfo(...)     static_cast<void>(0)
#define NpgsCoreTrace(...)    static_cast<void>(0)
#define NpgsCoreWarn(...)     static_cast<void>(0)

#define NpgsCritical(...)     static_cast<void>(0)
#define NpgsError(...)        static_cast<void>(0)
#define NpgsInfo(...)         static_cast<void>(0)
#define NpgsTrace(...)        static_cast<void>(0)
#define NpgsWarn(...)         static_cast<void>(0)

#endif // NPGS_ENABLE_CONSOLE_LOGGER || NPGS_ENABLE_FILE_LOGGER
