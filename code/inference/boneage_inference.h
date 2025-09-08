#pragma once

#include <condition_variable>
#include <cstddef>
#include <queue>
#include "context/context.h"
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <memory>

namespace inference {

class BoneAgeInferencer {
public:
    struct InferenceResult {
        // uint64_t task_id;
        std::string result_str;
    };

    using InferenceCallback = std::function<void(InferenceResult)>;

    struct InferenceTask {
        // uint64_t task_id;
        std::vector<unsigned char> raw_image_data;
        InferenceCallback on_complete;
    };

public:
    ~BoneAgeInferencer();

    BoneAgeInferencer(const BoneAgeInferencer&) = delete;
    BoneAgeInferencer& operator=(const BoneAgeInferencer&) = delete;

    static BoneAgeInferencer& GetInstance() {
        static BoneAgeInferencer instance;
        return instance;
    }

    void Init(const std::string& detection_model_path,
              const std::string& classification_model_path);
    
    void Shutdown();

    void PostInference(InferenceTask task);

private:
    BoneAgeInferencer();

    void Run_();

private:
    class InferencePipeline;
    std::unique_ptr<InferencePipeline> inferencer_;

    ctx::TaskRunnerTag task_runner_;
    std::atomic<bool> is_closed_{true};

    std::queue<InferenceTask> request_queue_; // 接收推理请求
    std::mutex requeset_mutex_;
    std::condition_variable request_cv_;

    static constexpr size_t kMaxInferenceBatchSize = 1;
    static constexpr size_t kMaxRequestQueueSize = 100;
};

}

#define INFERENCER inference::BoneAgeInferencer::GetInstance()