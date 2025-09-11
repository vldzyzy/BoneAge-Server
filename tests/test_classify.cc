#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <regex>
#include <chrono>
#include "nn/classify.h"
#include "logging/logger.h"
#include "bone_info.h"

namespace fs = std::filesystem;

// 简化的图像信息结构（仅用于性能测试）
struct ImageInfo {
    std::string file_path;
    int category_id;
};

// 递归加载所有图像文件
std::vector<ImageInfo> loadImagesFromFolder(const std::string& folderPath) {
    std::vector<ImageInfo> images;
    
    if (!fs::exists(folderPath) || !fs::is_directory(folderPath)) {
        std::cerr << "Error: Folder does not exist or is not a directory: " << folderPath << std::endl;
        return images;
    }
    
    int category_counter = 0;
    for (const auto& entry : fs::recursive_directory_iterator(folderPath)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            
            if (ext == ".png" || ext == ".jpg" || ext == ".jpeg") {
                ImageInfo info;
                info.file_path = entry.path().string();
                info.category_id = category_counter % 9; // 循环使用0-8的category_id
                images.push_back(info);
                category_counter++;
            }
        }
    }
    
    LOG_DEBUG("Successfully loaded {} images from folder.", images.size());
    return images;
}

// 分割数据为批次
struct BatchData {
    std::vector<cv::Mat> images;
    std::vector<int64_t> category_ids;
    std::vector<ImageInfo> infos;
};

std::vector<BatchData> splitIntoBatches(const std::vector<ImageInfo>& images, int batchSize) {
    std::vector<BatchData> batches;
    
    for (size_t i = 0; i < images.size(); i += batchSize) {
        size_t end = std::min(i + batchSize, images.size());
        
        BatchData batch;
        for (size_t j = i; j < end; ++j) {
            cv::Mat img = cv::imread(images[j].file_path);
            if (!img.empty()) {
                batch.images.push_back(img);
                batch.category_ids.push_back(images[j].category_id);
                batch.infos.push_back(images[j]);
            } else {
                LOG_DEBUG("Warning: Could not load image: {}", images[j].file_path);
            }
        }
        
        if (!batch.images.empty()) {
            batches.push_back(batch);
        }
    }
    
    return batches;
}

int main() {
    logging::InitConsole(logging::LogLevel::Debug);
    
    // 模型路径和图片文件夹路径
    std::string modelPath = "/workspace/BoneAge-Server/models/bone_maturity_predict.onnx";
    std::string imageFolder = "/workspace/BoneAge-Server/tests/images/joint";
    std::vector<int> testBatchSizes = {13, 26, 13, 26};  // 要测试的批次大小列表
    
    auto env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_VERBOSE, "classify_batch_test");
    
    try {
        // 创建分类器
        nn::MaturityClassifier classifier(env, modelPath, true, {112, 112});
        
        // 加载所有图像
        std::vector<ImageInfo> images = loadImagesFromFolder(imageFolder);
        if (images.empty()) {
            std::cerr << "FATAL: No valid images found in folder." << std::endl;
            return 1;
        }
        LOG_DEBUG("INFO: Loaded {} images for testing.", images.size());
        
        // 遍历测试不同批次大小
        for (int batchSize : testBatchSizes) {
            LOG_DEBUG("\n===== Testing batch size: {} =====", batchSize);
            
            // 性能测试 - 不需要保存结果
            
            // 分割图片为当前批次大小的子批次
            auto batches = splitIntoBatches(images, batchSize);
            LOG_DEBUG("INFO: Split into {} batches (last batch may be smaller).", batches.size());
            
            // 测量推理时间
            std::vector<double> batch_times;
            double total_inference_time = 0.0;
            int total_images = 0;
            
            // 逐批次推理并测量时间
            for (size_t b = 0; b < batches.size(); ++b) {
                const auto& batch = batches[b];
                LOG_DEBUG("INFO: Processing batch {}/{} (images: {})", b + 1, batches.size(), batch.images.size());
                
                // 开始计时
                auto start_time = std::chrono::high_resolution_clock::now();
                
                // 执行批次推理
                auto results = classifier.Classify(batch.images, batch.category_ids);
                
                // 结束计时
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                double batch_time_ms = duration.count() / 1000.0;
                
                batch_times.push_back(batch_time_ms);
                total_inference_time += batch_time_ms;
                total_images += batch.images.size();
                
                LOG_DEBUG("Batch {}: {} images processed in {:.2f} ms ({:.2f} ms/image)", 
                         b + 1, batch.images.size(), batch_time_ms, batch_time_ms / batch.images.size());
            }
            
            // 计算性能统计
            double avg_batch_time = total_inference_time / batches.size();
            double avg_time_per_image = total_inference_time / total_images;
            double throughput = total_images / (total_inference_time / 1000.0); // images per second
            
            LOG_DEBUG("===== Batch size {} Performance Results =====", batchSize);
            LOG_DEBUG("Total batches: {}", batches.size());
            LOG_DEBUG("Total images: {}", total_images);
            LOG_DEBUG("Total inference time: {:.2f} ms", total_inference_time);
            LOG_DEBUG("Average batch time: {:.2f} ms", avg_batch_time);
            LOG_DEBUG("Average time per image: {:.2f} ms", avg_time_per_image);
            LOG_DEBUG("Throughput: {:.2f} images/second", throughput);
            LOG_DEBUG("===== Finished batch size: {} =====", batchSize);
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}