#include "detect.h"
#include "onnxruntime_cxx_api.h"
#include <algorithm>
#include <bits/stdint-intn.h>
#include <opencv2/core.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/flann.hpp>
#include <numeric>
#include <execution>
#include "logging/logger.h"
#include <fmt/format.h>
#include <stdexcept>
#include <fmt/ranges.h>


namespace nn {

YOLO11Detector::YOLO11Detector(std::shared_ptr<Ort::Env> env, const std::string& model_path, bool use_gpu, const cv::Size& input_size, const std::vector<size_t>& warmup_batch_sizes)
    : env_(env),
      session_(nullptr),
      input_size_(input_size)
{
    LOG_DEBUG("initializing detect model");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(1);
    session_options.SetInterOpNumThreads(1);
    session_options.SetExecutionMode(ORT_SEQUENTIAL);
    if (use_gpu) {
        OrtCUDAProviderOptions cuda_options;
        cuda_options.device_id = 0;
        session_options.AppendExecutionProvider_CUDA(cuda_options);
    }
    // session_options.DisableMemPattern();
    session_ = Ort::Session(*env_, model_path.c_str(), session_options);

    // 获取输入/输出节点信息
    auto input_name_ptr = session_.GetInputNameAllocated(0, allocator_);
    input_name_str_ = input_name_ptr.get();
    
    Ort::TypeInfo input_type_info = session_.GetInputTypeInfo(0);
    auto input_tensor_info = input_type_info.GetTensorTypeAndShapeInfo();
    input_dims_ = input_tensor_info.GetShape();
    // 默认input size: [-1, 3, 640, 640]
    if (input_dims_[2] == -1) {
        input_dims_[2] = input_size.height;
    } else if (input_dims_[2] != input_size.height) {
        throw std::runtime_error("input size != model expected");
    }
    if (input_dims_[3] == -1) {
        input_dims_[3] = input_size.width;
    } else if (input_dims_[3] != input_size.width) {
        throw std::runtime_error("input size != model expected");
    }

    LOG_DEBUG("input shape: [{}]", fmt::join(input_dims_, ", "));

    auto output_name_ptr = session_.GetOutputNameAllocated(0, allocator_);
    output_name_str_ = output_name_ptr.get();
    
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

std::vector<std::vector<DetectionResult>> YOLO11Detector::Detect(const std::vector<cv::Mat>& images) {
    if (images.empty()) {
        return {};
    }
    LOG_DEBUG("running detection inference");
    const size_t batch_size = images.size();
    std::vector<cv::Mat> preprocessed_imgs(batch_size);
    std::vector<float> scales(batch_size);
    std::vector<cv::Size> original_sizes(batch_size);
    
    // std::vector<size_t> indices(batch_size);
    // std::iota(indices.begin(), indices.end(), 0);
    // std::for_each(std::execution::par, indices.begin(), indices.end(), [&](size_t i) {
    //     blobs[i] = Preprocess_(images[i], scales[i]);
    //     original_sizes[i] = images[i].size();
    // });
    for (int i = 0; i < batch_size; i++) {
        preprocessed_imgs[i] = Preprocess_(images[i], scales[i]);
        original_sizes[i] = images[i].size();
    }
    cv::Mat batch_blob = cv::dnn::blobFromImages(
        preprocessed_imgs,
        1.0 / 255.0, // 归一化到[0,1]
        input_size_,
        cv::Scalar(),           
        true, // BGR→RGB转换
        false,
        CV_32F
    );

    // prepare onnxruntime inputs
    const char* input_names[] = {input_name_str_.c_str()};
    const char* output_names[] = {output_name_str_.c_str()};

    std::vector<int64_t> batch_input_dims = input_dims_;
    batch_input_dims[0] = batch_size;

    std::vector<Ort::Value> input_tensors;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info, batch_blob.ptr<float>(), batch_blob.total(), batch_input_dims.data(), batch_input_dims.size()
    ));
    // 有且只有一个输出通道：output_tensors[0].size = [batch_size, num_attributes, num_proposals] = [bs, 7 + 4 = 11, 8400]
    LOG_DEBUG("running onnxruntime session");
    std::vector<Ort::Value> output_tensors;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        output_tensors = session_.Run(
            Ort::RunOptions{nullptr},
            input_names,
            input_tensors.data(),
            1,
            output_names,
            1
        );
    }
    LOG_DEBUG("onnxruntime session done");
    LOG_DEBUG("running postprocess");
    std::vector<std::vector<DetectionResult>> batch_results(batch_size);
    const float* output_data = output_tensors[0].GetTensorData<float>();
    const int num_attributes = output_dims_[1];
    const int num_proposals = output_dims_[2];
    // std::for_each(std::execution::par, indices.begin(), indices.end(),
    //     [&](size_t i) {
    //         const float* single_output_data = output_data + i * num_proposals * num_attributes;
    //         batch_results[i] = Postprocess_(single_output_data, scales[i], original_sizes[i]);
    //     }
    // );
    for (int i = 0; i < batch_size; i++) {
        const float* single_output_data = output_data + i * num_proposals * num_attributes;
        batch_results[i] = Postprocess_(single_output_data, scales[i], original_sizes[i]);
    }
    LOG_DEBUG("detection inference done, batch size: {}", batch_size);
    return batch_results;
}


cv::Mat YOLO11Detector::Preprocess_(const cv::Mat& image, float& scale) {
    LOG_DEBUG("original image size: {}x{}", image.cols, image.rows);
    // 确保输入是 3 通道 BGR
    cv::Mat input_image;
    if (image.channels() == 1) {
        cv::cvtColor(image, input_image, cv::COLOR_GRAY2BGR);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, input_image, cv::COLOR_BGRA2BGR);
    } else {
        input_image = image;
    }

    int ih = input_image.rows;
    int iw = input_image.cols;
    int th = input_size_.height;
    int tw = input_size_.width;

    scale = std::min(static_cast<float>(tw) / iw, static_cast<float>(th) / ih);
    int nw = iw * scale;
    int nh = ih * scale;

    cv::Mat resized_image;
    cv::resize(input_image, resized_image, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);
    
    cv::Mat letterbox_image(th, tw, CV_8UC3, cv::Scalar(114, 114, 114));
    resized_image.copyTo(letterbox_image(cv::Rect(0, 0, nw, nh)));

    return letterbox_image;
}

std::vector<DetectionResult> YOLO11Detector::Postprocess_(const float* output_data, float scale, const cv::Size& original_image_size) {
    std::vector<DetectionResult> results;
    
    cv::Mat output_buffer(output_dims_[1], output_dims_[2], CV_32F, const_cast<float*>(output_data));
    cv::transpose(output_buffer, output_buffer);

    std::vector<cv::Rect> boxes;
    std::vector<float> confidences;
    std::vector<int> class_ids;
    
    for (int i = 0; i < output_buffer.rows; i++) {
        float* row_data = output_buffer.ptr<float>(i);
        cv::Mat scores(1, output_dims_[1] - 4, CV_32F, row_data + 4);
        
        int class_id;
        double max_score;
        cv::Point class_id_point;
        cv::minMaxLoc(scores, 0, &max_score, 0, &class_id_point);
        
        if (max_score > kConfThreshold) {
            confidences.push_back(max_score);
            class_ids.push_back(class_id_point.x);
            
            float cx = row_data[0];
            float cy = row_data[1];
            float w = row_data[2];
            float h = row_data[3];
            
            int left = int((cx - 0.5 * w) / scale);
            int top = int((cy - 0.5 * h) / scale);
            int width = int(w / scale);
            int height = int(h / scale);
            
            boxes.push_back(cv::Rect(left, top, width, height));
        }
    }

    std::vector<int> nms_indices;
    cv::dnn::NMSBoxes(boxes, confidences, kConfThreshold, kNmsThreshold, nms_indices);

    for (int idx : nms_indices) {
        DetectionResult res;
        res.box = boxes[idx];
        res.confidence = confidences[idx];
        res.detect_class_id = class_ids[idx];
        results.push_back(res);
    }

    return results;
}

void YOLO11Detector::Warmup_(size_t batch_size) {
    LOG_DEBUG("warmup detect model with batch size: {}", batch_size);
    auto dummy_input_dims = input_dims_;
    dummy_input_dims[0] = batch_size;
    size_t dummy_input_size = dummy_input_dims[0] * dummy_input_dims[1] * dummy_input_dims[2] * dummy_input_dims[3];
    std::vector<float> dummy_input(dummy_input_size, 0.0f);
    
    std::vector<Ort::Value> dummy_input_tensors;
    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    dummy_input_tensors.push_back(Ort::Value::CreateTensor<float>(
        memory_info, dummy_input.data(), dummy_input.size(), dummy_input_dims.data(), dummy_input_dims.size()
    ));

    const char* input_names[] = {input_name_str_.c_str()};
    const char* output_names[] = {output_name_str_.c_str()};
    auto dummy_output_tensors = session_.Run(Ort::RunOptions{nullptr}, input_names, dummy_input_tensors.data(), 1, output_names, 1);

    output_dims_ = dummy_output_tensors[0].GetTensorTypeAndShapeInfo().GetShape();
}

}