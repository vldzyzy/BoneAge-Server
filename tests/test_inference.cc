#include <iostream>
#include <vector>
#include <string>
#include <filesystem> // C++17 标准库
#include <fstream>
#include <stdexcept>
#include <algorithm> // for std::transform
#include <unordered_set>
#include "inference/boneage_inference.h"

// C++17 a.k.a. std::filesystem
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
    std::string folder = "/workspace/BoneAge-Server/tests/images";
    std::string yolo_model_path = "/workspace/BoneAge-Server/models/yolo11m_detect.onnx";
    std::string cls_model_path = "/workspace/BoneAge-Server/models/bone_maturity_predict.onnx";

    // --- 1. 准备数据 ---
    auto image_buffers = ReadImagesFromFolder(folder);
    if (image_buffers.empty()) {
        std::cout << "\n未能从文件夹中读取任何图片。\n";
        return 1;
    }
    std::cout << "\n成功读取了 " << image_buffers.size() << " 张图片。\n";

    int batch_size = image_buffers.size();
    std::vector<InferenceTask> tasks;
    tasks.reserve(batch_size);
    for (int i = 0; i < batch_size; i++) {
        tasks.emplace_back(InferenceTask{static_cast<uint64_t>(i), std::move(image_buffers[i])});
    }

    // --- 2. 初始化并提交任务 ---
    auto& inferencer = BoneAgeInferencer::GetInstance();
    inferencer.Init(yolo_model_path, cls_model_path);

    for (auto& task : tasks) {
        inferencer.PostInference(std::move(task));
    }
    std::cout << "所有 " << batch_size << " 个任务已提交。\n";

    // --- 3. 使用 sleep 轮询方式等待并获取结果 ---
    std::vector<InferenceResult> final_results;
    final_results.reserve(batch_size);

    std::cout << "开始轮询等待推理结果...\n";
    while (final_results.size() < batch_size) {
        // 从推理器获取当前已完成的结果
        auto completed_tasks = inferencer.GetInferenceResult();

        if (!completed_tasks.empty()) {
            std::cout << "轮询成功: 收到 " << completed_tasks.size() << " 个新结果。\n";
            // 将收到的结果合并到最终结果向量中
            final_results.insert(final_results.end(),
                                 std::make_move_iterator(completed_tasks.begin()),
                                 std::make_move_iterator(completed_tasks.end()));
        } else {
            // 如果队列为空，主线程“睡”一会儿，避免空转浪费CPU
            std::cout << "结果队列为空，休眠1s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }

    // --- 4. 打印并关闭 ---
    // std::cout << "\n--- 所有推理结果 ---\n";
    // std::sort(final_results.begin(), final_results.end(), 
    //           [](const auto& a, const auto& b){ return a.task_id < b.task_id; });

    // for (const auto& res : final_results) {
    //     std::cout << "Task ID: " << res.task_id << "\n";
    //     std::cout << "JSON: " << res.result_str << std::endl;
    // }

    INFERENCER.Shutdown();

    return 0;
}