#include "stdafx.h"
#include "Logger.hpp"

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace Npgs::Util
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
        kCoreLogger_   = spdlog::stdout_color_mt("NPGS");
        kClientLogger_ = spdlog::stdout_color_mt("App");

        auto ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(kCoreLogger_->sinks()[0].get());
        if (ConsoleSink)
        {
            ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }

        ConsoleSink = dynamic_cast<spdlog::sinks::stdout_color_sink_mt*>(kClientLogger_->sinks()[0].get());
        if (ConsoleSink)
        {
            ConsoleSink->set_color(spdlog::level::trace, FOREGROUND_BLUE | FOREGROUND_INTENSITY);
        }
#elif defined(NPGS_ENABLE_FILE_LOGGER)
        kCoreLogger_   = spdlog::basic_logger_mt("NPGS", "NpgsCore.log", true);
        kClientLogger_ = spdlog::basic_logger_mt("App",  "Npgs.log",     true);

        kCoreLogger_->flush_on(spdlog::level::trace);
        kClientLogger_->flush_on(spdlog::level::trace);
#endif

        kCoreLogger_->set_level(spdlog::level::trace);
        kClientLogger_->set_level(spdlog::level::trace);
    }


    std::shared_ptr<spdlog::logger> FLogger::kCoreLogger_   = nullptr;
    std::shared_ptr<spdlog::logger> FLogger::kClientLogger_ = nullptr;
} // namespace Npgs::Util
