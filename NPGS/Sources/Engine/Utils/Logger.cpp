#include "stdafx.h"
#include "Logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Npgs::Utils
{
    void FLogger::Initialize()
    {
        // 修改日志格式模式
        // %^ %$ 是颜色开始和结束的标记
        // %l 是日志级别 (TRACE, DEBUG, INFO, WARN, ERROR, CRITICAL)
        // %T 是时间
        // %n 是日志器名称
        // %v 是实际的日志消息
        spdlog::set_pattern("[%T][%^%l%$] %n: %v");

#if defined(NPGS_ENABLE_CONSOLE_LOGGER)
        CoreLogger_   = spdlog::stdout_color_mt("NPGS");
        ClientLogger_ = spdlog::stdout_color_mt("App");

        auto ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(CoreLogger_->sinks()[0].get());
        if (ConsoleSink)
        {
            ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }

        ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(ClientLogger_->sinks()[0].get());
        if (ConsoleSink)
        {
            ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }
#elif defined(NPGS_ENABLE_FILE_LOGGER)
        CoreLogger_   = spdlog::basic_logger_mt("NPGS", "NpgsCore.log", true);
        ClientLogger_ = spdlog::basic_logger_mt("App",  "Npgs.log",     true);

        CoreLogger_->flush_on(spdlog::level::trace);
        ClientLogger_->flush_on(spdlog::level::trace);
#endif

        CoreLogger_->set_level(spdlog::level::trace);
        ClientLogger_->set_level(spdlog::level::trace);
    }


    std::shared_ptr<spdlog::logger> FLogger::CoreLogger_   = nullptr;
    std::shared_ptr<spdlog::logger> FLogger::ClientLogger_ = nullptr;
} // namespace Npgs::Utils
