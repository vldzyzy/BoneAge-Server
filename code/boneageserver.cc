#include "http/httpapplication.h"
#include "http/httpcontext.h"
#include "http/middleware.h"
#include "inference/boneage_inference.h"
#include "net/eventloop.h"
#include "net/inetaddress.h"

#include <iostream>

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/rotating_file_sink.h>

#include "logging/logger.h"
#include "CLI/CLI.hpp" 
#include "config.h"

#include <fmt/format.h>

int main(int argc, char** argv) {
    CLI::App app{"bone age server"};
    app.set_version_flag("-v,--version", "0.1.0");

    Config config;

    app.add_option("--ip", config.server_ip, "IP address the server listens on");
    app.add_option("--port", config.port, "Port the server listens on");
    app.add_option("--io-threads", config.num_io_threads, "Number of IO threads");
    app.add_option("--infer-threads", config.num_infer_threads, "Number of infer threads");

    app.add_option("--static-dir", config.static_root_path, "Path to static files directory");
    
    app.add_option("--yolo-model", config.yolo_model_path, "Path to YOLO detection model");
    app.add_option("--cls-model", config.cls_model_path, "Path to classification model");

    app.add_option("--log-path", config.log_path, "Base path for log files");
    
    std::map<std::string, logging::LogLevel> log_level_map {
        {"trace", logging::LogLevel::Trace},
        {"debug", logging::LogLevel::Debug},
        {"info", logging::LogLevel::Info},
        {"warn", logging::LogLevel::Warn},
        {"error", logging::LogLevel::Error},
        {"critical", logging::LogLevel::Critical},
        {"off", logging::LogLevel::Off}
    };
    app.add_option("--log-level", config.log_level, "Set log level (trace, debug, info, ...)")
        ->transform(CLI::CheckedTransformer(log_level_map, CLI::ignore_case));

    app.set_config("-c,--config", "config.toml", "Read an toml file", true);

    CLI11_PARSE(app, argc, argv);

    std::string log_level_str;
    switch(config.log_level) {
        case logging::LogLevel::Trace: log_level_str = "trace"; break;
        case logging::LogLevel::Debug: log_level_str = "debug"; break;
        case logging::LogLevel::Info: log_level_str = "info"; break;
        case logging::LogLevel::Warn: log_level_str = "warn"; break;
        case logging::LogLevel::Error: log_level_str = "error"; break;
        case logging::LogLevel::Critical: log_level_str = "critical"; break;
        case logging::LogLevel::Off: log_level_str = "off"; break;
        default: log_level_str = "unknown";
    }

    logging::Init(config.log_path, config.log_level);
    LOG_INFO("Server configuration loaded successfully. "
         "IP: {}, Port: {}, IO Threads: {}, Infer Threads: {}, Static Dir: {}"
         "YOLO Model: {}, Classification Model: {}, "
         "Log Path: {}, Log Level: {}",
         config.server_ip,
         config.port,
         config.num_io_threads,
         config.num_infer_threads,
         config.static_root_path,
         config.yolo_model_path,
         config.cls_model_path,
         config.log_path,
         log_level_str);

    INFERENCER.Init(config.num_infer_threads, config.yolo_model_path, config.cls_model_path);
    LOG_INFO("Inference engine initialized successfully.");

    net::InetAddress listen_addr(config.server_ip, config.port);
    http::HttpApplication http_app(std::move(listen_addr), config.static_root_path, "bone_age");

    http_app.SetThreadNum(config.num_io_threads);

    LOG_INFO("Server listening...");
    http_app.Start();

    INFERENCER.Shutdown();

    return 0;
}
