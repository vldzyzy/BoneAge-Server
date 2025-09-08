#include "logging/logger.h"

std::shared_ptr<spdlog::logger> g_logger;

namespace logging {

spdlog::level::level_enum toSpdlogLevel(LogLevel level) {
    switch (level) {
        case LogLevel::Trace:    return spdlog::level::trace;
        case LogLevel::Debug:    return spdlog::level::debug;
        case LogLevel::Info:     return spdlog::level::info;
        case LogLevel::Warn:     return spdlog::level::warn;
        case LogLevel::Error:    return spdlog::level::err;
        case LogLevel::Critical: return spdlog::level::critical;
        case LogLevel::Off:      return spdlog::level::off;
    }
    return spdlog::level::trace; // 默认
}

void Init(const std::string& log_dir, 
                 LogLevel level,
                 int rotation_hour,
                 int rotation_minute) 
{
    try {
        fs::path log_path = fs::path(log_dir) / "log.log";
        auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_path, rotation_hour, rotation_minute);
        
        g_logger = std::make_shared<spdlog::logger>("server_logger", daily_sink);

        spdlog::register_logger(g_logger);

        // [时间][级别] 消息
        g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        
        g_logger->set_level(toSpdlogLevel(level));

        spdlog::flush_every(std::chrono::seconds(10));

        g_logger->flush_on(spdlog::level::err);

    } catch (const spdlog::spdlog_ex& ex) {
        fprintf(stderr, "Log initialization failed: %s\n", ex.what());
    }
}

}