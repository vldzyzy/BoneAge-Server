#pragma once

#include "logging/logger.h"
#include <string>
#include <vector>

struct Config {
    std::string server_ip ;
    int port;
    int num_io_threads;
    int num_infer_threads;

    std::string static_root_path;

    std::string yolo_model_path;
    std::string cls_model_path;

    std::string log_path;
    logging::LogLevel log_level;
};
