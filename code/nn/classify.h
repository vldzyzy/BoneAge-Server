#pragma once

#include <onnxruntime_cxx_api.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace nn {

struct ClassificationResult {
    int category_id; // 9 个
    int maturity_stage;
    float confidence;
};

class MaturityClassifier {
public:
    MaturityClassifier(std::shared_ptr<Ort::Env> env,
                       const std::string& model_path,
                       bool use_gpu, 
                       const cv::Size& input_size);

    MaturityClassifier(const MaturityClassifier&) = delete;
    MaturityClassifier& operator=(const MaturityClassifier&) = delete;

    /**
     * @brief 对一批图片及其类别 ID 执行分类
     * @param images 输入的 OpenCV 图像向量
     * @param category_ids 对应的类别 ID 向量
     * @return 返回结果向量，results[i] 对应 images[i] 和 category_ids[i] 的分类结果
     */
    std::vector<ClassificationResult> Classify(const std::vector<cv::Mat>& images, const std::vector<int64_t>& category_ids);

public:
    static constexpr size_t kMaxBatchSize = 21;

private:
    cv::Mat Preprocess_(const cv::Mat& image);
    ClassificationResult Postprocess_(const float* output_data, int category_id);
    void softmax_(std::vector<float>& data) const;

    std::shared_ptr<Ort::Env> env_;
    Ort::Session session_;
    Ort::AllocatorWithDefaultOptions allocator_;

    std::string image_input_name_str_;
    std::string category_id_input_name_str_;
    
    std::string output_name_str_;
    
    cv::Size input_size_;
    std::vector<int64_t> image_input_dims_;
    std::vector<int64_t> category_id_input_dims_;
    std::vector<int64_t> output_dims_;
};
}