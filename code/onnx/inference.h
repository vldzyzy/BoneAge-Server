#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <future>

#include <opencv4/opencv2/opencv.hpp>

#include "../log/log.h"
#include "../types/types.h"

extern BlockQueue<inferenceTask> inferenceQueue;

static inline std::unordered_map<std::string, std::string> categoryMap {
    {"DIP1", "DIPFirst"},
    {"DIP2", "DIP"},
    {"DIP3", "DIP"},
    {"DIP4", "DIP"},
    {"DIP5", "DIP"},
    {"MIP1", "MIP"},
    {"MIP2", "MIP"},
    {"MIP3", "MIP"},
    {"MIP4", "MIP"},
    {"MIP5", "MIP"},
    {"PIP1", "PIPFirst"},
    {"PIP2", "PIP"},
    {"PIP3", "PIP"},
    {"PIP4", "PIP"},
    {"PIP5", "PIP"},
    {"FMCP", "MCPFirst"},
    {"MCP1", "MCP"},
    {"MCP2", "MCP"},
    {"MCP3", "MCP"},
    {"MCP4", "MCP"},
    {"MCP5", "MCP"}
};


static void generate_image_regions(
    const cv::Mat& original_image, // 原始图像
    const std::unordered_map<std::string, cv::Rect>& selected_boxes,
    std::vector<cv::Mat>& images,
    std::vector<std::string>& categories
) {

    // 清空输出容器
    images.clear();
    categories.clear();

    // 遍历所有检测框
    for (const auto& [category, rect] : selected_boxes) {
        // 检查矩形是否在图像范围内
        if (rect.x >= 0 && rect.y >= 0 &&
            rect.x + rect.width <= original_image.cols &&
            rect.y + rect.height <= original_image.rows) 
        {
            // 提取图像区域
            cv::Mat roi = original_image(rect);
            images.push_back(std::move(roi));

            // 类别转换
            auto it = categoryMap.find(category);
            if(it != categoryMap.end()) {
                categories.push_back(it->second);
            }
            else{
                categories.push_back(category);
            }
        } else {
            // 可选的错误处理逻辑
            LOG_ERROR("Invalid rectangle:");
        }
    }
}

void inference();