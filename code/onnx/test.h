#pragma once
#include <string>

// 模型推理接口：注意传入的 image 为 string_view，不经过复制
static std::string inference(std::string_view image, std::string text) {
    // 计算文件大小（字节）
    size_t fileSizeInBytes = image.length();

    // 转换为 KB（1 KB = 1024 字节）
    double fileSizeInKB = static_cast<double>(fileSizeInBytes) / 1024.0;

    std::string error_ = "false";


    // 构造 JSON 格式的字符串，包含 "image" 和 "输入的文本" 两个字段
    std::string jsonResult = "{\"image\": " + std::to_string(fileSizeInKB) +
                             ", \"输入的文本\": \"" + text + "\"" +
                             ", \"error\": false}";

    return jsonResult;
}