#include "classify.h"
#include <execution>
#include <numeric>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include "logging/logger.h"
#include <fmt/ranges.h>
#include "bone_info.h"

namespace nn {

MaturityClassifier::MaturityClassifier(
    std::shared_ptr<Ort::Env> env, 
    const std::string& model_path,
    bool use_gpu, 
    const cv::Size& input_size,
    const std::vector<size_t>& warmup_batch_sizes)
    : env_(env),
      session_(nullptr),
      input_size_(input_size)
{
    LOG_DEBUG("classify model initializing");
    Ort::SessionOptions session_options;
    if (use_gpu) {
        OrtCUDAProviderOptions cuda_options;
        cuda_options.device_id = 0;
        session_options.AppendExecutionProvider_CUDA(cuda_options);
    }
    session_ = Ort::Session(*env_, model_path.c_str(), session_options);

    // 获取节点信息
    LOG_DEBUG("获取节点信息");
    auto image_input_name_holder = session_.GetInputNameAllocated(0, allocator_);
    image_input_name_str_ = image_input_name_holder.get();
    const char* image_input_name_ptr_ = image_input_name_str_.c_str();
    
    auto category_id_input_name_holder = session_.GetInputNameAllocated(1, allocator_);
    category_id_input_name_str_ = category_id_input_name_holder.get();
    const char* category_id_input_name_ptr_ = category_id_input_name_str_.c_str();

    auto output_name_holder = session_.GetOutputNameAllocated(0, allocator_);
    output_name_str_ = output_name_holder.get();
    const char* output_name_ptr_ = output_name_str_.c_str();
    LOG_DEBUG("get input type");
    Ort::TypeInfo image_type_info = session_.GetInputTypeInfo(0);
    auto image_tensor_info = image_type_info.GetTensorTypeAndShapeInfo();
    image_input_dims_ = image_tensor_info.GetShape();
    if (image_input_dims_[2] == -1) { 
        image_input_dims_[2] = input_size_.height;
    }
    if (image_input_dims_[3] == -1) {
        image_input_dims_[3] = input_size_.width;
    }

    Ort::TypeInfo category_id_type_info = session_.GetInputTypeInfo(1);
    auto category_id_tensor_info = category_id_type_info.GetTensorTypeAndShapeInfo();
    category_id_input_dims_ = category_id_tensor_info.GetShape();

    LOG_DEBUG("input image shape: [{}]", fmt::join(image_input_dims_, ", "));
    LOG_DEBUG("input category shape: [{}]", fmt::join(category_id_input_dims_, ", "));

    // warmup
    if (warmup_batch_sizes.empty()) {
        Warmup_(1);
    } else {
        for (size_t batch_size : warmup_batch_sizes) {
            Warmup_(batch_size);
        }
    }
    LOG_DEBUG("output tensors shape: [{}]", fmt::join(output_dims_, ", "));
}

void MaturityClassifier::Warmup_(size_t batch_size) {
    LOG_DEBUG("warmup with batch size: {}", batch_size);
    auto dummy_image_input_dims = image_input_dims_;
    dummy_image_input_dims[0] = batch_size;
    auto dummy_category_input_dims = category_id_input_dims_;
    dummy_category_input_dims[0] = batch_size;

    size_t dummy_image_size = dummy_image_input_dims[0] * dummy_image_input_dims[1] * dummy_image_input_dims[2] * dummy_image_input_dims[3];
    std::vector<float> dummy_image_input(dummy_image_size, 0.0f);
    std::vector<int64_t> dummy_category_id_input(batch_size, 0);

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    std::vector<Ort::Value> dummy_input_tensors;
    dummy_input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info, dummy_image_input.data(), dummy_image_input.size(), dummy_image_input_dims.data(), dummy_image_input_dims.size()
    ));
    dummy_input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, dummy_category_id_input.data(), dummy_category_id_input.size(), dummy_category_input_dims.data(), dummy_category_input_dims.size()
    ));

    const char* input_names[] = {image_input_name_str_.c_str(), category_id_input_name_str_.c_str()};
    const char* output_names[] = {output_name_str_.c_str()};
    auto dummy_output_tensors = session_.Run(Ort::RunOptions{nullptr}, input_names, dummy_input_tensors.data(), 2, output_names, 1);
    output_dims_ = dummy_output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
}

void MaturityClassifier::softmax_(std::vector<float>& data) const {
    if (data.empty()) return;

    float max_val = *std::max_element(data.begin(), data.end());
    float sum = 0.0f;
    
    std::for_each(data.begin(), data.end(), [&](float& val) {
        val = std::exp(val - max_val);
        sum += val;
    });

    if (sum > 0.0f) {
        std::for_each(data.begin(), data.end(), [sum](float& val) {
            val /= sum;
        });
    }
}

ClassificationResult MaturityClassifier::Postprocess_(const float* output_data, int category_id) {
    std::vector<float> relevant_scores(output_data, output_data + BoneInfo::GetMaturityRange(category_id));
    softmax_(relevant_scores);

    int predicted_index = 0;
    float max_score = relevant_scores[0];

    for (int i = 1; i < relevant_scores.size(); ++i) {
        if (relevant_scores[i] > max_score) {
            max_score = relevant_scores[i];
            predicted_index = i;
        }
    }

    return {category_id, predicted_index + 1, max_score};
}

std::vector<ClassificationResult> MaturityClassifier::Classify(const std::vector<cv::Mat>& images, const std::vector<int64_t>& category_ids) {
    if (images.empty()) {
        return {};
    }
    LOG_DEBUG("running classification inference");

    const size_t batch_size = images.size();

    // prepare onnxruntime inputs
    cv::Mat image_blob = cv::dnn::blobFromImages(images, 1.0 / 255.0, input_size_, cv::Scalar(), true, false, CV_32F);
    std::vector<int64_t> batch_image_dims = image_input_dims_;
    batch_image_dims[0] = batch_size;

    std::vector<int64_t> category_ids_input = category_ids;
    std::vector<int64_t> batch_category_id_dims = category_id_input_dims_;
    batch_category_id_dims[0] = batch_size;

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<Ort::Value> input_tensors;
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info, image_blob.ptr<float>(), image_blob.total(), batch_image_dims.data(), batch_image_dims.size()
    ));
    input_tensors.push_back(Ort::Value::CreateTensor<int64_t>(
        memory_info, category_ids_input.data(), category_ids_input.size(), batch_category_id_dims.data(), batch_category_id_dims.size()
    ));
    const char* input_names[] = {image_input_name_str_.c_str(), category_id_input_name_str_.c_str()};
    const char* output_names[] = {output_name_str_.c_str()};
    // run inference
    LOG_DEBUG("running onnxruntime session");
    std::vector<Ort::Value> output_tensors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_tensors = session_.Run(Ort::RunOptions{nullptr}, input_names, input_tensors.data(), 2, output_names, 1);
    }
    LOG_DEBUG("onnxruntime session done");
    LOG_DEBUG("running postprocess");
    std::vector<ClassificationResult> batch_results(batch_size);
    const float* output_data_ptr = output_tensors[0].GetTensorData<float>();
    const int num_total_stages = output_dims_[1];

    for (int i = 0; i < batch_size; i++) {
        const float* single_output_data = output_data_ptr + i * num_total_stages;
        batch_results[i] = Postprocess_(single_output_data, category_ids[i]);
    }
    LOG_DEBUG("classification inference done, batch size: {}", batch_size);

    return batch_results;
}

} // namespace nn