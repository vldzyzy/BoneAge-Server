#include <onnxruntime/onnxruntime_cxx_api.h>
#include <opencv4/opencv2/opencv.hpp>
#include <opencv4/opencv2/imgproc/imgproc.hpp>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cuda_fp16.h>

#include "../json/CJsonObject.hpp"
#include "inference.h"

// 启用cuda
class BoneAgeCls {
public:
    // 删除拷贝构造函数和赋值运算符
    BoneAgeCls(const BoneAgeCls&) = delete;
    BoneAgeCls& operator=(const BoneAgeCls&) = delete;

    // 析构函数
    ~BoneAgeCls();
    
    // 单例实例获取器
    static BoneAgeCls* instance();
 
    // 核心函数
    void init(const char* modelPath);
    bool predict(const std::vector<cv::Mat>& images, const std::vector<std::string>& categories);
    bool predict(std::vector<inferenceTask>& tasks);

    // 转换结果为JSON
    std::vector<int> getPredictions();

    // 存储类别信息
    std::map<std::string, int> categoryToIdx;
    std::map<int, std::string> idxToCategory;
    std::map<std::string, int> categoryRanges;
    std::map<std::string, std::string> categoryMap;
    
private:
    // 公共辅助函数
    bool blobFromImage(const cv::Mat& image, std::vector<float>& blob);
    bool warmUpSession();
    bool preProcess(const cv::Mat& image, cv::Mat& processedImage);
    bool postProcess(const std::vector<Ort::Value>& outputTensors, const std::vector<int>& categories);
    
    static BoneAgeCls* m_instance;
    static std::mutex m_mutex;
    
    Ort::Env env;
    Ort::Session* session;
    Ort::RunOptions options;
    std::vector<const char*> inputNodeNames;
    std::vector<const char*> outputNodeNames;
    std::vector<int> imgSize;
    
    // 输入/输出数据
    std::vector<int> inputCategories;
    std::vector<std::vector<float>> outputResults;
    
    // 私有构造函数，用于单例模式
    BoneAgeCls();
    
    // 辅助函数 - 实现细节
    bool createSession(const char* modelPath);
    bool tensorProcess(const std::vector<cv::Mat>& images, const std::vector<int>& categories, std::vector<Ort::Value>& inputTensors);
    bool runSession(const std::vector<Ort::Value>& inputTensors, std::vector<Ort::Value>& outputTensors);
};
