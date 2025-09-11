#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include "nn/detect.h"
#include "utils/utils.h"
#include "logging/logger.h"

namespace fs = std::filesystem;

// 检查文件是否为支持的图片格式
bool isImageFile(const std::string& filename) {
    std::vector<std::string> extensions = {".jpg", ".jpeg", ".png", ".bmp", ".tif", ".tiff"};
    std::string ext = fs::path(filename).extension().string();
    
    // 转换为小写以便不区分大小写检查
    for (char& c : ext) {
        c = tolower(c);
    }
    
    for (const auto& e : extensions) {
        if (ext == e) {
            return true;
        }
    }
    return false;
}

// 读取文件夹中所有图片
std::vector<cv::Mat> loadImagesFromFolder(const std::string& folderPath) {
    std::vector<cv::Mat> images;
    
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        std::cerr << "Error: Folder does not exist or is not a directory: " << folderPath << std::endl;
        return images;
    }
    
    for (const auto& entry : fs::directory_iterator(folderPath)) {
        if (entry.is_regular_file() && isImageFile(entry.path().string())) {
            cv::Mat img = cv::imread(entry.path().string());
            if (!img.empty()) {
                images.push_back(img);
                LOG_DEBUG("Loaded image: {} ({}x{})", entry.path().string(), img.cols, img.rows);
            } else {
                std::cerr << "Warning: Could not load image: " << entry.path() << std::endl;
            }
        }
    }
    
    LOG_DEBUG("Successfully loaded {} images from folder.", images.size());
    return images;
}


// 分割图片列表为指定批次大小的子列表
std::vector<std::vector<cv::Mat>> splitIntoBatches(const std::vector<cv::Mat>& images, int batchSize) {
    std::vector<std::vector<cv::Mat>> batches;
    for (size_t i = 0; i < images.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, images.size());
        batches.emplace_back(images.begin() + i, images.begin() + end);
    }
    return batches;
}

int main() {
    logging::InitConsole(logging::LogLevel::Debug);
    // 模型路径和图片文件夹路径
    std::string modelPath = "/workspace/BoneAge-Server/models/yolo11m_detect.onnx";
    std::string imageFolder = "/workspace/BoneAge-Server/tests/images";
    std::vector<int> testBatchSizes = {1, 8};  // 要测试的批次大小列表

    auto env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "yolo_batch_test");
    nn::YOLO11Detector yolo11(env, modelPath, true, {640, 640}, {1, 8});

    // 加载所有图片（共16张）
    std::vector<cv::Mat> images = loadImagesFromFolder(imageFolder);
    if (images.empty()) {
        std::cerr << "FATAL: No valid images found in folder." << std::endl;
        return 1;
    }
    LOG_DEBUG("INFO: Loaded {} images for testing.", images.size());

    // 遍历测试不同批次大小
    for (int batchSize : testBatchSizes) {
        LOG_DEBUG("\n===== Testing batch size: {} =====", batchSize);
        
        // 创建批次输出目录（避免结果混淆）
        std::string outputDir = "batch_" + std::to_string(batchSize) + "_results/";
        fs::create_directory(outputDir);  // 确保目录存在

        // 分割图片为当前批次大小的子批次
        auto batches = splitIntoBatches(images, batchSize);
        LOG_DEBUG("INFO: Split into {} batches (last batch may be smaller).", batches.size());

        // 逐批次推理并处理结果
        size_t totalProcessed = 0;
        for (size_t b = 0; b < batches.size(); ++b) {
            const auto& batch = batches[b];
            LOG_DEBUG("INFO: Processing batch {}/{} (images: {})", b + 1, batches.size(), batch.size());

            // 执行批次推理
            auto results = yolo11.Detect(batch);

            // 验证结果数量
            if (results.size() != batch.size()) {
                std::cerr << "WARNING: Batch " << b + 1 << " result mismatch (" 
                          << results.size() << " vs " << batch.size() << ")" << std::endl;
                continue;
            }

            // 保存当前批次的结果
            for (size_t i = 0; i < batch.size(); ++i) {
                cv::Mat resultImg = batch[i].clone();
                utils::DrawBoxes(resultImg, results[i]);

                // 文件名格式：{输出目录}/result_{总序号}.jpg
                std::string outputFilename = outputDir + "result_" + std::to_string(totalProcessed + i) + ".jpg";
                if (cv::imwrite(outputFilename, resultImg)) {
                    LOG_DEBUG("SUCCESS: Saved to {}", outputFilename);
                } else {
                    std::cerr << "WARNING: Failed to save " << outputFilename << std::endl;
                }
            }
            totalProcessed += batch.size();
        }

        LOG_DEBUG("===== Finished batch size: {} (total processed: {}) =====", batchSize, totalProcessed);
    }

    return 0;
}