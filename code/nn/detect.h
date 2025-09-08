#pragma once

#include <string>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>

namespace nn {

struct DetectionResult {
    cv::Rect box;
    float confidence;
    int detect_class_id; // 7个类别
};

class YOLO11Detector {
public:
    YOLO11Detector(std::shared_ptr<Ort::Env> env, const std::string& model_path, bool use_gpu, const cv::Size& input_size);

    YOLO11Detector(const YOLO11Detector&) = delete;
    YOLO11Detector& operator=(const YOLO11Detector&) = delete;

    /**
     * @brief 对一批图片执行目标检测 (Batch Inference)
     * @param images 输入的 OpenCV 图像向量 (std::vector<cv::Mat>)
     * @return 返回一个结果向量的向量。results[i] 对应 images[i] 的检测结果。
     */
    std::vector<std::vector<DetectionResult>> Detect(const std::vector<cv::Mat>& image);

private:
    cv::Mat Preprocess_(const cv::Mat& image, float& scale);

    std::vector<DetectionResult> Postprocess_(const float* output_data, float scale, const cv::Size& original_image_size);

public:
    static constexpr float kConfThreshold = 0.5f;
    static constexpr float kNmsThreshold = 0.45f;
    static constexpr size_t kMaxBatchSize = 1;

private:
    std::shared_ptr<Ort::Env> env_;
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;

    std::string input_name_str_;
    std::string output_name_str_;
    cv::Size input_size_;
    std::vector<int64_t> input_dims_;
    std::vector<int64_t> output_dims_;
};
}