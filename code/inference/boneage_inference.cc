#include "boneage_inference.h"
#include "context/context.h"
#include "nn/detect.h"
#include "nn/classify.h"
#include <algorithm>
#include <opencv2/opencv.hpp>
#include <tbb/task.h>
#include "logging/logger.h"
#include "bone_info.h"
#include "onnxruntime_c_api.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include "http/httpresponse.h"
#include "net/eventloop.h"

namespace nlohmann {
    template <>
    struct adl_serializer<cv::Rect> {
        static void to_json(json& j, const cv::Rect& rect) {
            j = {
                {"x", rect.x},
                {"y", rect.y},
                {"width", rect.width},
                {"height", rect.height}
            };
        }
    };
}

namespace inference {

using json = nlohmann::json;

struct BoneDetail {
    std::string joint;
    cv::Rect box;
    int category_id;
    int maturity_stage;
};

struct HandDetail {
    std::vector<BoneDetail> bones_detail;
    bool is_valid;
};

// BoneDetail 的序列化
void to_json(json& j, const BoneDetail& bone) {
    j = {
        {"joint", bone.joint},
        {"box", bone.box},
        {"category_id", bone.category_id},
        {"maturity_stage", bone.maturity_stage}
    };
}

// HandDetail 的序列化
void to_json(json& j, const HandDetail& hand) {
    j = {
        {"is_valid", hand.is_valid},
        {"bones_detail", hand.bones_detail}
    };
}

class BoneAgeInferencer::InferencePipeline {
public:
    InferencePipeline(std::shared_ptr<Ort::Env> env, 
                      const std::string& detection_model_path,
                      const std::string& classification_model_path)
        : detector_(env, detection_model_path, true, {640, 640}, {1}),
          classifier_(env, classification_model_path, true, {112, 112}, {12, 13, 14}) 
    {}

    std::vector<HandDetail> inference(const std::vector<cv::Mat>& images) {
        int batch_size = images.size();
        LOG_DEBUG("batch size: {}", batch_size);
        std::vector<HandDetail> batch_processed;
        batch_processed.reserve(batch_size);

        // 检测, 提取
        std::vector<std::vector<nn::DetectionResult>> detection_result = detector_.Detect(images);
        for (int i = 0; i < batch_size; i++) {
            LOG_DEBUG("image {} detect {} boxes", i, detection_result[i].size());
            auto hand_detail = GetHandDetail(detection_result[i]);
            batch_processed.emplace_back(std::move(hand_detail));
        }
        // 分类
        std::vector<cv::Mat> batch_joint_images;
        batch_joint_images.reserve(batch_size * BoneInfo::kKeyJoints.size());
        std::vector<int64_t> batch_category_ids;
        batch_category_ids.reserve(batch_size * BoneInfo::kKeyJoints.size());

        for (int i = 0; i < batch_size; i++) {
            // 构造输入
            auto& hand_detail = batch_processed[i];
            auto joint_counts = hand_detail.bones_detail.size(); // 正常应该是13个，但如果检测出错，个数不确定
            for (int j = 0; j < joint_counts; j++) {
                auto& bone_detail = hand_detail.bones_detail[j];
                // 构造图像 
                cv::Rect clipped_box = bone_detail.box & cv::Rect(0, 0, images[i].cols, images[i].rows);
                batch_joint_images.emplace_back(images[i](clipped_box));
                // 种类
                batch_category_ids.emplace_back(bone_detail.category_id);
            }
        }
        
        std::vector<nn::ClassificationResult> batch_classify_result = classifier_.Classify(batch_joint_images, batch_category_ids);

        // std::vector<nn::ClassificationResult> batch_classify_result;
        // batch_classify_result.reserve(batch_joint_images.size());

        // for (int i = 0; i < batch_joint_images.size(); i++) {
        //     std::vector<cv::Mat> single_joint_image{batch_joint_images[i]};
        //     std::vector<int> single_category_id{batch_category_ids[i]};
        //     batch_classify_result.emplace_back(classifier_.Classify(single_joint_image, single_category_id)[0]);
        // }

        // 填充结果
        int offset = 0;
        for (int i = 0; i < batch_size; i++) {
            auto& hand_detail = batch_processed[i];
            auto joint_counts = hand_detail.bones_detail.size();
            for (int j = 0; j < joint_counts; j++) {
                hand_detail.bones_detail[j].maturity_stage = batch_classify_result[offset + j].maturity_stage;
            }
            offset += joint_counts;
        }

        // for (int i = 0; i < batch_size; ++i) {
        //     const auto& hand_detail = batch_processed[i];
        //     if (!hand_detail.is_valid) {
        //         LOG_DEBUG("Image[{}] result: Invalid (detection incomplete).", i);
        //         continue;
        //     }
        //     LOG_DEBUG("Image[{}] result: Valid, contains {} bones.", i, hand_detail.bones_detail.size());
        //     for (const auto& bone : hand_detail.bones_detail) {
        //         LOG_DEBUG("  - Joint: {}, Box[{}, {}, {}x{}], Stage: {}",
        //                 bone.joint, bone.box.x, bone.box.y, bone.box.width, bone.box.height, bone.maturity_stage);
        //     }
        // }
        return batch_processed;
    }

    HandDetail GetHandDetail(const std::vector<nn::DetectionResult> detection_results) {
        std::map<int, std::vector<cv::Rect>> bones;
        bool success = true;  // 记录是否完全解析成功

        // 根据detect_class_id，从低到高排序检测结果
        for (const auto& detect_result : detection_results) {
            bones[detect_result.detect_class_id].emplace_back(detect_result.box);
        }
        // LOG_DEBUG("extracting key joints from {} detection result", bones.size());

        // 存储解析结果
        HandDetail hand_detail;
        hand_detail.bones_detail.reserve(BoneInfo::kKeyJoints.size());
        // 开始提取joint
        int detect_class_id = 0;
        // 1. 提取Radius, Ulna, MCPFirst, joint id: 0, 1, 2. 对应detect_class_id: 0, 1, 2
        for (; detect_class_id <= 2; detect_class_id++) {
            auto it = bones.find(detect_class_id);
            if (it != bones.end()) {
                if(it->second.size() == BoneInfo::DetectGetExpectedCountById(detect_class_id)) { 
                    hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(detect_class_id)), it->second[0], detect_class_id, -1});
                    // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(detect_class_id), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(detect_class_id));
                }
                else {
                    for(size_t i = 0; i < it->second.size(); ++i) {
                        std::string joint_name = std::string(BoneInfo::JointGetNameById(detect_class_id)) + std::to_string(i);
                        hand_detail.bones_detail.push_back({joint_name, it->second[i], detect_class_id, -1});
                    }
                    LOG_ERROR("{} count mismatch: expected {} got {}", 
                            BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::DetectGetExpectedCountById(detect_class_id), it->second.size());
                    success = false;
                }
            }
        }

        // 2. 提取mcpthird, mcpfifth, joint id: 3, 4; 对应detect_class_id: 3, category_id: 3
        detect_class_id = 3;
        auto it = bones.find(detect_class_id);
        if (it != bones.end()) {
            size_t size = it->second.size();
            // 按x坐标降序排序
            std::sort(it->second.begin(), it->second.end(),
                    [](const cv::Rect& a, const cv::Rect& b) { return a.x > b.x; });
            if(size == BoneInfo::DetectGetExpectedCountById(detect_class_id)) {
                // 存储第二和第四个骨头
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(3)), it->second[1], 3, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(3), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(3));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(4)), it->second[3], 3, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(4), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(3));
            }
            else {
                // 检测结果有错误不匹配, 全都存了, 推理后返回用户, 让用户手动删除错误的
                for (size_t i = 0; i < size; ++i) {
                    std::string joint_name = "MCP" + std::to_string(i);
                    hand_detail.bones_detail.push_back({std::string(joint_name), it->second[i], 3, -1});
                }
                LOG_ERROR("{} count mismatch: expected {} got {}", 
                        BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::DetectGetExpectedCountById(detect_class_id), it->second.size());
                success = false;
            }
        }

        // 3. 提取pipfirst, pipthird, pipfifth, joint id: 5, 6, 7; 对应detect_class_id: 4, category_id: 4, 5
        detect_class_id = 4;
        it = bones.find(detect_class_id);
        if (it != bones.end()) {
            size_t size = it->second.size();
            // 按x坐标降序排序
            std::sort(it->second.begin(), it->second.end(),
                    [](const cv::Rect& a, const cv::Rect& b) { return a.x > b.x; });
            if(size == BoneInfo::DetectGetExpectedCountById(detect_class_id)) {
                // 存储第1, 3, 5个骨头
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(5)), it->second[0], 4, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(5), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(4));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(6)), it->second[2], 5, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(6), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(5));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(7)), it->second[4], 5, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(7), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(5));
            }
            else {
                // 检测结果有错误不匹配, 全都存了, 推理后返回用户, 让用户手动删除错误的
                for (size_t i = 0; i < size; ++i) {
                    std::string joint_name = "PIP" + std::to_string(i);
                    hand_detail.bones_detail.push_back({joint_name, it->second[i], 5, -1}); // 都当PIP进行推理
                }
                LOG_ERROR("{} count mismatch: expected {} got {}", 
                        BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::DetectGetExpectedCountById(detect_class_id), it->second.size());
                success = false;
            }
        }

        // 4. 提取mipthird, mipfifth, joint id: 8, 9; 对应detect_class_id: 5, category_id: 6
        detect_class_id = 5;
        it = bones.find(detect_class_id);
        if (it != bones.end()) {
            size_t size = it->second.size();
            // 按x坐标降序排序
            std::sort(it->second.begin(), it->second.end(),
                    [](const cv::Rect& a, const cv::Rect& b) { return a.x > b.x; });
            if(size == BoneInfo::DetectGetExpectedCountById(detect_class_id)) {
                // 存储第2, 4个骨头
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(8)), it->second[1], 6, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(8), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(6));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(9)), it->second[3], 6, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(9), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(6));
            }
            else {
                // 检测结果有错误不匹配, 全都存了, 推理后返回用户, 让用户手动删除错误的
                for (size_t i = 0; i < size; ++i) {
                    std::string joint_name = "MIP" + std::to_string(i);
                    hand_detail.bones_detail.push_back({joint_name, it->second[i], 6, -1});
                }
                LOG_ERROR("{} count mismatch: expected {} got {}", 
                        BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::DetectGetExpectedCountById(detect_class_id), it->second.size());
                success = false;
            }
        }

        // 5. 提取dipfirst, dipthird, dipfifth, joint id: 10, 11, 12; 对应detect_class_id: 6, category_id: 7, 8
        detect_class_id = 6;
        it = bones.find(detect_class_id);
        if (it != bones.end()) {
            size_t size = it->second.size();
            // 按x坐标降序排序
            std::sort(it->second.begin(), it->second.end(),
                    [](const cv::Rect& a, const cv::Rect& b) { return a.x > b.x; });
            if(size == BoneInfo::DetectGetExpectedCountById(detect_class_id)) {
                // 存储第1, 3, 5个骨头
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(10)), it->second[0], 7, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(10), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(7));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(11)), it->second[2], 8, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(11), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(8));
                hand_detail.bones_detail.push_back({std::string(BoneInfo::JointGetNameById(12)), it->second[4], 8, -1});
                // LOG_DEBUG("extracted {} from detect class {}, category id: {}", BoneInfo::JointGetNameById(12), BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::ClsGetNameById(8));
            }
            else {
                // 检测结果有错误不匹配, 全都存了, 推理后返回用户, 让用户手动删除错误的
                for (size_t i = 0; i < size; ++i) {
                    std::string joint_name = "DIP" + std::to_string(i);
                    hand_detail.bones_detail.push_back({joint_name, it->second[i], 8, -1}); //都当DIP
                }
                LOG_ERROR("{} count mismatch: expected {} got {}", 
                        BoneInfo::DetectGetNameById(detect_class_id), BoneInfo::DetectGetExpectedCountById(detect_class_id), it->second.size());
                success = false;
            }
        }
        hand_detail.is_valid = success;
        // LOG_DEBUG("done, {} joints extracted", hand_detail.bones_detail.size());
        return hand_detail;
    }

private:
    nn::YOLO11Detector detector_;
    nn::MaturityClassifier classifier_;
};

BoneAgeInferencer::BoneAgeInferencer() = default;

BoneAgeInferencer::~BoneAgeInferencer() = default;

void BoneAgeInferencer::Init(size_t thread_count, const std::string& detection_model_path,
                                 const std::string& classification_model_path)
{
    auto env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "BoneAgeApp");

    inferencer_ = std::make_unique<InferencePipeline>(env, 
                                                      detection_model_path, 
                                                      classification_model_path);

    is_closed_.store(false);
    thread_count_ = thread_count;
    task_runner_ = NEW_PARALLEL_RUNNER(3, thread_count_);
    for (size_t i = 0; i < thread_count_; ++i) {
        POST_TASK(task_runner_, [this]() {
            Run_();
        });
    }
}

void BoneAgeInferencer::Shutdown() {
    is_closed_.store(true);
    request_cv_.notify_all();
    
    inferencer_.reset();
}

void BoneAgeInferencer::PostInference(InferenceTask task) {
    std::unique_lock<std::mutex> lock(requeset_mutex_);
    request_cv_.wait(lock, [this]() { return request_queue_.size() < kMaxRequestQueueSize || is_closed_.load(); });
    if (is_closed_.load()) {
        return;
    }
    request_queue_.emplace(std::move(task));
    request_cv_.notify_one();
}

void BoneAgeInferencer::Run_() {
    while (true) {
        std::vector<InferenceTask> batch_tasks;
        batch_tasks.reserve(kMaxInferenceBatchSize);
        {
            std::unique_lock<std::mutex> lock(requeset_mutex_);
            request_cv_.wait(lock, [this]() { return !request_queue_.empty() || is_closed_.load();});
            if (is_closed_.load()) {
                return;
            }
            size_t total_task_count = request_queue_.size();
            size_t batch_size = 1;

            if (total_task_count > thread_count_) {
                size_t desired_batch_size = (total_task_count + thread_count_ - 1) / thread_count_;
                if (desired_batch_size > 1) {
                    batch_size = 1;
                    while ((batch_size << 1) <= desired_batch_size) {
                        batch_size <<= 1;
                    }
                }
            }
            batch_size = std::min({batch_size, (size_t)kMaxInferenceBatchSize, total_task_count});

            for (size_t i = 0; i < batch_size; i++) {
                batch_tasks.emplace_back(std::move(request_queue_.front()));
                request_queue_.pop();
            }
            request_cv_.notify_one();
        }   
        std::vector<cv::Mat> batch_images;
        std::vector<InferenceTask> valid_tasks; // 成功解码的任务
        batch_images.reserve(batch_tasks.size());
        valid_tasks.reserve(batch_tasks.size());

        for (auto& task : batch_tasks) {
            // 从内存解码图像
            cv::Mat image;
            try {
                image = cv::imdecode(task.raw_image_data, cv::IMREAD_COLOR);
            } catch (...) {
                // 捕获所有异常，确保服务器不崩溃
                // LOG_ERROR("Task ID {} failed to decode image.", task.task_id);
                LOG_ERROR("Failed to decode image.");
                LOG_ERROR("image size: {}", task.raw_image_data.size());
                InferenceResult result;
                // result.task_id = task.task_id;
                task.on_complete(std::move(result));
                continue;
            }

            if (!image.empty()) {
                batch_images.push_back(image);
                valid_tasks.push_back(std::move(task));
            } else {
                // LOG_ERROR("Task ID {} failed to decode image.", task.task_id);
                LOG_ERROR("Failed to decode image.");
                LOG_ERROR("image size: {}", task.raw_image_data.size());
                InferenceResult result;
                // result.task_id = task.task_id;
                task.on_complete(std::move(result));
            }
        }

        std::vector<HandDetail> hands_detail = inferencer_->inference(batch_images);
        LOG_INFO("thread id: {}, Inferred {} task", std::hash<std::thread::id>{}(std::this_thread::get_id()), batch_tasks.size());

        for (size_t i = 0; i < valid_tasks.size(); ++i) {
            InferenceResult result;
            // result.task_id = valid_tasks[i].task_id;
            result.result_str = json(hands_detail[i]).dump();
            valid_tasks[i].on_complete(std::move(result));
        }
    }
}

}