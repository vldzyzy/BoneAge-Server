#include "BoneAgeCls.h"

// 静态成员初始化

// 单例模式实现
BoneAgeCls* BoneAgeCls::instance() {
    static BoneAgeCls instance;
    return &instance;
}

// 构造函数
BoneAgeCls::BoneAgeCls() : 
    env(Ort::Env(ORT_LOGGING_LEVEL_WARNING, "BoneAgeCls")),
    session(nullptr),
    options(Ort::RunOptions()) {
    
    // 初始化图像大小
    imgSize = {112, 112};
    
    // 初始化类别映射
    categoryToIdx = {
        {"DIP", 0}, 
        {"DIPFirst", 1}, 
        {"MCP", 2}, 
        {"MCPFirst", 3}, 
        {"MIP", 4}, 
        {"PIP", 5}, 
        {"PIPFirst", 6}, 
        {"Radius", 7}, 
        {"Ulna", 8}
    };
    
    // 初始化反向映射
    for (const auto& pair : categoryToIdx) {
        idxToCategory[pair.second] = pair.first;
    }
    
    // 初始化类别范围
    categoryRanges = {
        {"DIP", 11}, 
        {"DIPFirst", 11}, 
        {"MCP", 10}, 
        {"MCPFirst", 11}, 
        {"MIP", 12}, 
        {"PIP", 12}, 
        {"PIPFirst", 12}, 
        {"Radius", 14}, 
        {"Ulna", 12}
    };

    // 类别映射
    categoryMap = {
        {"DIP1", "DIPFirst"},
        {"DIP3", "DIP"},
        {"DIP5", "DIP"},
        {"MIP3", "MIP"},
        {"MIP5", "MIP"},
        {"PIP1", "PIPFirst"},
        {"PIP3", "PIP"},
        {"PIP5", "PIP"},
        {"MCPFirst", "MCPFirst"},
        {"MCP3", "MCP"},
        {"MCP5", "MCP"}
    };

}

// 析构函数
BoneAgeCls::~BoneAgeCls() {
    // if (session) {
    //     delete session;
    //     session = nullptr;
    // }
}

// 初始化模型
void BoneAgeCls::init(const char* modelPath) {
    // 创建ONNX会话
    if (!createSession(modelPath)) {
        throw std::runtime_error("创建ONNX会话失败");
    }
    
    // 预热会话
    if (!warmUpSession()) {
        throw std::runtime_error("预热ONNX会话失败");
    }
}

// 创建ONNX会话
bool BoneAgeCls::createSession(const char* modelPath) {
    try {
        // 会话选项
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        
        OrtCUDAProviderOptions cudaOptions;
        cudaOptions.device_id = 0;
        sessionOptions.AppendExecutionProvider_CUDA(cudaOptions);
        
        // 创建会话
        session = new Ort::Session(env, modelPath, sessionOptions);
        
        // 获取输入和输出节点名称
        Ort::AllocatorWithDefaultOptions allocator;
        
        // 获取输入节点名称
        size_t numInputNodes = session->GetInputCount();
        inputNodeNames.resize(numInputNodes);
        
        for (size_t i = 0; i < numInputNodes; i++) {
            auto inputName = session->GetInputNameAllocated(i, allocator);
            size_t nameLength = strlen(inputName.get()) + 1;
            char* name = new char[nameLength];
            strcpy(name, inputName.get());
            inputNodeNames[i] = name;
        }
        
        // 获取输出节点名称
        size_t numOutputNodes = session->GetOutputCount();
        outputNodeNames.resize(numOutputNodes);
        
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto outputName = session->GetOutputNameAllocated(i, allocator);
            size_t nameLength = strlen(outputName.get()) + 1;
            char* name = new char[nameLength];
            strcpy(name, outputName.get());
            outputNodeNames[i] = name;
        }
        
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX 异常: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "标准异常: " << e.what() << std::endl;
        return false;
    }
}

// 使用虚拟输入预热会话
bool BoneAgeCls::warmUpSession() {
    try {
        // 创建一个虚拟图像和类别用于预热
        cv::Mat dummyImage(imgSize[0], imgSize[1], CV_8UC3, cv::Scalar(0, 0, 0));
        std::vector<cv::Mat> images = {dummyImage};
        std::vector<std::string> categories = {{"DIP"}}; // DIP类别
        
        // 使用虚拟数据运行预测
        return predict(images, categories);
    } catch (const std::exception& e) {
        std::cerr << "预热异常: " << e.what() << std::endl;
        return false;
    }
}

// 预处理输入图像
bool BoneAgeCls::preProcess(const cv::Mat& image, cv::Mat& processedImage) {
    try {
        // 将图像调整为所需大小(112x112)
        cv::resize(image, processedImage, cv::Size(imgSize[1], imgSize[0]), 0, 0, cv::INTER_LINEAR);
        
        // 转换为浮点型
        processedImage.convertTo(processedImage, CV_32FC3, 1.0 / 255.0);
        
        // 归一化
        cv::Scalar mean(0.5, 0.5, 0.5);
        cv::Scalar std(0.5, 0.5, 0.5);
        
        // 为每个通道创建临时矩阵
        std::vector<cv::Mat> channels(3);
        cv::split(processedImage, channels);
        
        // 对每个通道应用归一化
        for (int i = 0; i < 3; i++) {
            channels[i] = (channels[i] - mean[i]) / std[i];
        }
        
        // 合并通道
        cv::merge(channels, processedImage);
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "预处理异常: " << e.what() << std::endl;
        return false;
    }
}

// 将图像转换为ONNX blob格式(HWC到CHW)
bool BoneAgeCls::blobFromImage(const cv::Mat& image, std::vector<float>& blob) {
    try {
        // 使用正确的大小初始化blob
        blob.resize(3 * imgSize[0] * imgSize[1]);
        
        // HWC到CHW
        for (int c = 0; c < 3; c++) {
            for (int h = 0; h < imgSize[0]; h++) {
                for (int w = 0; w < imgSize[1]; w++) {
                    blob[c * imgSize[0] * imgSize[1] + h * imgSize[1] + w] = 
                        image.at<cv::Vec3f>(h, w)[c];
                }
            }
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Blob创建异常: " << e.what() << std::endl;
        return false;
    }
}

// 处理输入张量
bool BoneAgeCls::tensorProcess(const std::vector<cv::Mat>& images, const std::vector<int>& categories, std::vector<Ort::Value>& inputTensors) {
    try {
        // 检查图像和类别是否具有相同的大小
        if (images.size() != categories.size()) {
            std::cerr << "图像和类别的数量不匹配" << std::endl;
            return false;
        }
        
        // 创建内存信息
        Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        
        // 处理并准备图像张量
        size_t batchSize = images.size();
        std::vector<float> imageBlob(batchSize * 3 * imgSize[0] * imgSize[1]);
        
        for (size_t i = 0; i < batchSize; i++) {
            // 预处理图像
            cv::Mat processedImage;
            if (!preProcess(images[i], processedImage)) {
                return false;
            }
            
            // 转换为blob
            std::vector<float> singleBlob;
            if (!blobFromImage(processedImage, singleBlob)) {
                return false;
            }
            
            // 复制到批处理blob
            std::copy(singleBlob.begin(), singleBlob.end(), 
                      imageBlob.begin() + i * 3 * imgSize[0] * imgSize[1]);
        }
        
        // 定义图像张量的形状
        std::vector<int64_t> imageShape = {static_cast<int64_t>(batchSize), 3, 
                                           static_cast<int64_t>(imgSize[0]), 
                                           static_cast<int64_t>(imgSize[1])};
        
        // 创建图像张量
        inputTensors.push_back(Ort::Value::CreateTensor<float>(
            memoryInfo, imageBlob.data(), imageBlob.size(),
            imageShape.data(), imageShape.size()));
        
        // 准备类别张量
        std::vector<int64_t> categoryTensor(categories.begin(), categories.end());
        std::vector<int64_t> categoryShape = {static_cast<int64_t>(batchSize)};
        
        // 创建类别张量
        inputTensors.push_back(Ort::Value::CreateTensor<int64_t>(
            memoryInfo, categoryTensor.data(), categoryTensor.size(),
            categoryShape.data(), categoryShape.size()));
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "张量处理异常: " << e.what() << std::endl;
        return false;
    }
}

// 运行推理会话
bool BoneAgeCls::runSession(const std::vector<Ort::Value>& inputTensors, std::vector<Ort::Value>& outputTensors) {
    try {
        // 运行推理
        outputTensors = session->Run(Ort::RunOptions{nullptr}, 
                                     inputNodeNames.data(), 
                                     inputTensors.data(), 
                                     inputTensors.size(), 
                                     outputNodeNames.data(), 
                                     outputNodeNames.size());
        
        return true;
    } catch (const Ort::Exception& e) {
        std::cerr << "ONNX Runtime异常: " << e.what() << std::endl;
        return false;
    }
}

// 主预测函数
bool BoneAgeCls::predict(const std::vector<cv::Mat>& images, const std::vector<std::string>& categories_) {
    try {
        std::vector<int> categories;
        categories.reserve(categories_.size());
        for (const auto& cat : categories_) {
            auto it = categoryToIdx.find(cat);
            if (it != categoryToIdx.end()) {
                categories.push_back(it->second);
            } else {
                std::cerr << "未找到 '" << cat << "' 的映射。" << std::endl;
                return false;
            }
        }
        // 处理输入张量
        std::vector<Ort::Value> inputTensors;
        if (!tensorProcess(images, categories, inputTensors)) {
            return false;
        }
        
        // 运行推理
        std::vector<Ort::Value> outputTensors;
        if (!runSession(inputTensors, outputTensors)) {
            return false;
        }
        
        // 处理输出
        if (!postProcess(outputTensors, categories)) {
            return false;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "预测异常: " << e.what() << std::endl;
        return false;
    }
}

// 推理一个batch的inferenceTask
bool BoneAgeCls::predict(std::vector<inferenceTask>& tasks) {
    try {
        // TODO:
        
    } catch (const std::exception& e) {
        std::cerr << "预测异常: " << e.what() << std::endl;
        return false;
    }
}

// 后处理模型输出
bool BoneAgeCls::postProcess(const std::vector<Ort::Value>& outputTensors, const std::vector<int>& categories) {
    try {
        // 清除先前的结果
        outputResults.clear();
        inputCategories = categories;
        
        // 获取输出张量数据
        const float* outputData = outputTensors[0].GetTensorData<float>();
        
        // 获取张量形状
        auto outputShape = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape();
        int64_t batchSize = outputShape[0];
        int64_t numClasses = outputShape[1]; // 应该是15
        
        // 处理每个批次项
        for (int64_t i = 0; i < batchSize; i++) {
            std::vector<float> result(numClasses);
            for (int64_t j = 0; j < numClasses; j++) {
                result[j] = outputData[i * numClasses + j];
            }
            outputResults.push_back(result);
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "后处理异常: " << e.what() << std::endl;
        return false;
    }
}

// 修改函数名和返回类型
std::vector<int> BoneAgeCls::getPredictions() {
    std::vector<int> predictions; // 用于存储预测结果的向量

    try {
        // 处理每个结果
        for (size_t i = 0; i < outputResults.size(); i++) {
            // 获取类别信息
            int categoryIdx = inputCategories[i];
            std::string categoryName = idxToCategory[categoryIdx];
            int categoryRange = categoryRanges[categoryName];

            // 找到最可能的成熟度等级
            std::vector<float> logits(categoryRange);
            for (int j = 0; j < categoryRange; j++) {
                logits[j] = outputResults[i][j];
            }

            // 应用 softmax 获取概率
            float maxVal = *std::max_element(logits.begin(), logits.end());
            float sumExp = 0.0f;
            std::vector<float> probabilities(categoryRange);

            for (int j = 0; j < categoryRange; j++) {
                probabilities[j] = std::exp(logits[j] - maxVal);
                sumExp += probabilities[j];
            }

            for (int j = 0; j < categoryRange; j++) {
                probabilities[j] /= sumExp;
            }

            // 找到预测的成熟度等级（从1开始索引）
            int predictedLevel = std::distance(probabilities.begin(), 
                                               std::max_element(probabilities.begin(), probabilities.end())) + 1;

            // 添加预测结果到向量
            predictions.push_back(predictedLevel);
        }
    } catch (const std::exception& e) {
        std::cerr << "预测转换异常: " << e.what() << std::endl;
        // 返回空向量表示错误
        return std::vector<int>();
    }

    return predictions;
}