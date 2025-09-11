#include <iostream>
#include <vector>
#include <string>
#include <filesystem> // C++17 标准库
#include <fstream>
#include <stdexcept>
#include <algorithm> // for std::transform
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>
#include "inference/boneage_inference.h"
#include "logging/logger.h"

namespace fs = std::filesystem;

/**
 * @brief 将字符串转换为全小写
 * @param str 输入字符串
 * @return 全小写字符串
 */
static std::string ToLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return str;
}

/**
 * @brief 将指定文件完整读取为二进制字节向量
 * @param file_path 文件的完整路径
 * @return 包含文件所有字节的 std::vector<unsigned char>
 * @throws std::runtime_error 如果文件无法打开或读取
 */
static std::vector<unsigned char> ReadFileBytes(const fs::path& file_path) {
    // 以二进制模式和at-end模式打开文件
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);

    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + file_path.string());
    }

    // 获取文件大小
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg); // 将指针移回文件开头

    // 创建vector并一次性读取所有数据
    std::vector<unsigned char> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        throw std::runtime_error("无法读取文件: " + file_path.string());
    }

    return buffer;
}

/**
 * @brief 读取指定文件夹内的所有图片文件，并返回它们的原始二进制数据
 * @param folder_path 目标文件夹路径
 * @return 一个向量，其中每个元素是单张图片的二进制数据向量
 * @throws std::filesystem::filesystem_error 如果路径无效或不是一个目录
 */
std::vector<std::vector<unsigned char>> ReadImagesFromFolder(const fs::path& folder_path) {
    std::vector<std::vector<unsigned char>> all_image_data;

    // 1. 定义我们认可的图片文件扩展名（全小写）
    const std::unordered_set<std::string> valid_extensions = {
        ".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"
    };

    try {
        // 2. 检查路径是否存在且是否为目录
        if (!fs::exists(folder_path) || !fs::is_directory(folder_path)) {
            std::cerr << "错误: 路径不存在或不是一个文件夹: " << folder_path << std::endl;
            return all_image_data;
        }

        std::cout << "正在读取文件夹: " << folder_path << std::endl;

        // 3. 遍历目录中的所有条目
        for (const auto& entry : fs::directory_iterator(folder_path)) {
            // 4. 检查是否为普通文件
            if (entry.is_regular_file()) {
                const fs::path& current_path = entry.path();
                std::string extension = ToLower(current_path.extension().string());

                // 5. 检查文件扩展名是否在我们的白名单中
                if (valid_extensions.count(extension)) {
                    try {
                        // 6. 读取文件内容并添加到结果向量中
                        all_image_data.push_back(ReadFileBytes(current_path));
                        std::cout << "  已读取: " << current_path.filename() << std::endl;
                    } catch (const std::runtime_error& e) {
                        std::cerr << "  读取失败: " << current_path.filename() << " - " << e.what() << std::endl;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "文件系统错误: " << e.what() << std::endl;
    }

    return all_image_data;
}

// --- 使用示例 ---
int main() {
    using namespace inference;
    std::string folder = "/workspace/BoneAge-Server/tests/images/hand";
    std::string yolo_model_path = "/workspace/BoneAge-Server/models/yolo11m_detect.onnx";
    std::string cls_model_path = "/workspace/BoneAge-Server/models/bone_maturity_predict.onnx";

    logging::InitConsole();

    // --- 1. 准备数据 ---
    auto image_buffers = ReadImagesFromFolder(folder);
    if (image_buffers.empty()) {
        std::cout << "\n未能从文件夹中读取任何图片。\n";
        return 1;
    }
    std::cout << "\n成功读取了 " << image_buffers.size() << " 张图片。\n";

    int image_num = image_buffers.size();
    
    // --- 2. 初始化推理器 ---
    auto& inferencer = BoneAgeInferencer::GetInstance();
    inferencer.Init(4, yolo_model_path, cls_model_path);

    // --- 3. 使用回调函数收集结果 ---
    std::vector<BoneAgeInferencer::InferenceResult> final_results;
    std::mutex results_mutex;
    std::condition_variable results_cv;
    int completed_count = 0;

    auto callback = [&](BoneAgeInferencer::InferenceResult result) {
        std::lock_guard<std::mutex> lock(results_mutex);
        final_results.push_back(std::move(result));
        completed_count++;
        std::cout << "收到推理结果\n";
        results_cv.notify_one();
    };

    std::unique_lock<std::mutex> lock(results_mutex);

    // --- 4. 低负载测试：一张一张间隔提交 ---
    std::cout << "\n=== 开始低负载测试 ===\n";
    int low_load_count = std::min(5, (int)image_buffers.size()); // 最多测试5张
    for (int i = 0; i < low_load_count; i++) {
        BoneAgeInferencer::InferenceTask task;
        task.raw_image_data = image_buffers[i]; // 复制数据用于后续高负载测试
        task.on_complete = callback;
        inferencer.PostInference(std::move(task));
        std::cout << "提交第 " << (i+1) << " 个低负载任务\n";
        
        // 间隔500ms再提交下一个
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // 等待低负载任务完成
    std::cout << "等待低负载任务完成...\n";
    results_cv.wait(lock, [&]() { return completed_count >= low_load_count; });
    std::cout << "低负载测试完成，已完成 " << completed_count << " 个任务\n";
    
    // --- 5. 高负载测试：批量快速提交 ---
     std::cout << "\n=== 开始高负载测试 ===\n";
     int high_load_count = (int)image_buffers.size() * 3; // 提交3倍数量的任务
     for (int i = 0; i < high_load_count; i++) {
         BoneAgeInferencer::InferenceTask task;
         // 循环使用图像数据
         task.raw_image_data = image_buffers[i % image_buffers.size()];
        task.on_complete = callback;
        inferencer.PostInference(std::move(task));
        
        // 每10个任务打印一次进度
        if ((i + 1) % 10 == 0) {
            std::cout << "已提交 " << (i + 1) << "/" << high_load_count << " 个高负载任务\n";
        }
    }
    std::cout << "所有 " << high_load_count << " 个高负载任务已提交。\n";
    
    int total_expected = low_load_count + high_load_count;

    // --- 6. 等待所有结果完成 ---
    std::cout << "等待所有高负载任务完成...\n";
    results_cv.wait(lock, [&]() { return completed_count >= total_expected; });
    std::cout << "所有任务完成！总共完成 " << completed_count << " 个任务\n";

    // --- 7. 打印结果并关闭 ---
     std::cout << "\n--- 推理结果统计 ---\n";
     std::cout << "低负载任务数: " << low_load_count << "\n";
     std::cout << "高负载任务数: " << high_load_count << "\n";
     std::cout << "总完成任务数: " << final_results.size() << "\n";
    
    // 只打印前几个结果作为示例
    int sample_count = std::min(3, (int)final_results.size());
    std::cout << "\n--- 示例结果 (前" << sample_count << "个) ---\n";
    for (int i = 0; i < sample_count; i++) {
        std::cout << "结果 " << (i+1) << ": " << final_results[i].result_str << "\n";
    }

    inferencer.Shutdown();

    return 0;
}