#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"
#include <filesystem>

#include <memory>

extern std::shared_ptr<spdlog::logger> g_logger;

namespace logging {

namespace fs = std::filesystem;

enum class LogLevel {
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Critical,
    Off
};

spdlog::level::level_enum toSpdlogLevel(LogLevel level);

void Init(const std::string& log_dir, 
                 LogLevel level = LogLevel::Info,
                 int rotation_hour = 0,
                 int rotation_minute = 0);

inline void SetLevel(LogLevel level) {
    if (g_logger) {
        g_logger->set_level(toSpdlogLevel(level));
    }
}

} // namespace logging

#define LOG_TRACE(...)    if(g_logger) { g_logger->trace(__VA_ARGS__); }
#define LOG_DEBUG(...)    if(g_logger) { g_logger->debug(__VA_ARGS__); }
#define LOG_INFO(...)     if(g_logger) { g_logger->info(__VA_ARGS__); }
#define LOG_WARN(...)     if(g_logger) { g_logger->warn(__VA_ARGS__); }
#define LOG_ERROR(...)    if(g_logger) { g_logger->error(__VA_ARGS__); }
#define LOG_CRITICAL(...) if(g_logger) { g_logger->critical(__VA_ARGS__); }