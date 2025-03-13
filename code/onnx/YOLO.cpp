#include "YOLO.h"
#include <regex>

#define benchmark   // 性能测试开关
#define USE_CUDA
#define min(a,b)         (((a) < (b)) ? (a) : (b))


#ifdef USE_CUDA
namespace Ort
{
    template<>
    struct TypeToTensorType<half> { static constexpr ONNXTensorElementDataType type = ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16; };
}
#endif

YOLO_V8::YOLO_V8() {
    
}

YOLO_V8::~YOLO_V8(){

}

YOLO_V8* YOLO_V8::instance() {
    static YOLO_V8 instance; // C++11保证静态局部变量线程安全
    return &instance;
}

void YOLO_V8::init(const char* modelPath) {
    DL_INIT_PARAM params;
    params.rectConfidenceThreshold = 0.5;
    params.iouThreshold = 0.5;
    params.modelPath = modelPath;   // 相对于程序的运行目录
    params.imgSize = { 640, 640 };
    params.cudaEnable = true;
    // GPU FP32 inference
    params.modelType = YOLO_DETECT_V8;
    // GPU FP16 inference
    //Note: change fp16 onnx model
    //params.modelType = YOLO_DETECT_V8_HALF;
    createSession(params);
    LOG_INFO("Create session");
}

// 将 ORT 日志级别映射到自定义日志级别
int YOLO_V8::ConvertOrtLogLevel(OrtLoggingLevel ortLevel) {
    switch (ortLevel) {
        case ORT_LOGGING_LEVEL_FATAL: return 3;
        case ORT_LOGGING_LEVEL_ERROR: return 3;
        case ORT_LOGGING_LEVEL_WARNING: return 2;
        case ORT_LOGGING_LEVEL_INFO: return 1;
        case ORT_LOGGING_LEVEL_VERBOSE: return 0;
        default: return 0;
    }
}

//日志回调函数
void ORT_API_CALL YOLO_V8::MyLogCallback(
    void* param, 
    OrtLoggingLevel severity, 
    const char* category, 
    const char* logid, 
    const char* code_location, 
    const char* message
) {
    // 忽略 param（除非需要上下文）
    int userLevel = ConvertOrtLogLevel(severity);

    LOG_BASE(userLevel, "%s", message);
}

// 实现批量图像转blob方法
template<typename T>
char* YOLO_V8::blobFromImages(const std::vector<cv::Mat>& imgs, T& iBlob) {
    if (imgs.empty()) {
        return "Empty images array";
    }
    
    int channels = 3; // 我们确保输入图像是3通道
    int imgHeight = imgSize[0];
    int imgWidth = imgSize[1];
    
    // 遍历每个批次的图像
    for (size_t b = 0; b < imgs.size(); b++) {
        const cv::Mat& img = imgs[b];
        if (img.channels() != channels || img.rows != imgHeight || img.cols != imgWidth) {
            LOG_WARN("Image dimensions mismatch in batch. Expected %dx%dx%d, got %dx%dx%d", 
                    imgWidth, imgHeight, channels, img.cols, img.rows, img.channels());
            continue;
        }
        
        // 计算当前批次图像的起始偏移
        size_t batchOffset = b * channels * imgWidth * imgHeight;
        
        // 转换当前图像到blob
        for (int c = 0; c < channels; c++) {
            for (int h = 0; h < imgHeight; h++) {
                for (int w = 0; w < imgWidth; w++) {
                    // 对于转换类型，确保使用正确的类型转换
                    if constexpr (std::is_same<typename std::remove_pointer<T>::type, float>::value) {
                        // 对于 float 类型
                        iBlob[batchOffset + c * imgWidth * imgHeight + h * imgWidth + w] = 
                            static_cast<float>(img.at<cv::Vec3b>(h, w)[c]) / 255.0f;
                    } 
                    else {
                        // 对于 half 类型
                        iBlob[batchOffset + c * imgWidth * imgHeight + h * imgWidth + w] = 
                            static_cast<typename std::remove_pointer<T>::type>(
                                static_cast<float>(img.at<cv::Vec3b>(h, w)[c]) / 255.0f);
                    }
                }
            }
        }
    }
    
    return RET_OK;
}

/**
 * @brief 预处理输入图像以适配 YOLO 模型的输入要求。
 *
 * 根据模型类型（如检测、分类）对图像进行颜色空间转换、缩放、裁剪等操作，
 * 并将结果调整为模型期望的尺寸和格式。
 *
 * @param iImg 输入图像（OpenCV Mat）
 * @param iImgSize 模型输入尺寸（宽、高）
 * @param oImg 输出预处理后的图像（OpenCV Mat）
 * @return 返回操作状态码（RET_OK 表示成功）
 */
char* YOLO_V8::preProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg, float& oResizeScale)
{
    switch (modelType)
    {
    case YOLO_DETECT_V8:
    case YOLO_POSE:
    case YOLO_DETECT_V8_HALF:
    case YOLO_POSE_V8_HALF://LetterBox
    {
        float resizeScale;
        if (iImg.cols >= iImg.rows)
        {
            resizeScale = iImg.cols / (float)iImgSize.at(0);
            cv::resize(iImg, oImg, cv::Size(iImgSize.at(0), int(iImg.rows / resizeScale)));
        }
        else
        {
            resizeScale = iImg.rows / (float)iImgSize.at(0);
            cv::resize(iImg, oImg, cv::Size(int(iImg.cols / resizeScale), iImgSize.at(1)));
        }
        oResizeScale = resizeScale; // 返回当前图像的缩放比例
        cv::Mat tempImg = cv::Mat::zeros(iImgSize.at(0), iImgSize.at(1), CV_8UC3);
        oImg.copyTo(tempImg(cv::Rect(0, 0, oImg.cols, oImg.rows)));
        oImg = tempImg;
        break;
    }
    case YOLO_CLS://CenterCrop
    {
        int h = iImg.rows;
        int w = iImg.cols;
        int m = min(h, w);
        int top = (h - m) / 2;
        int left = (w - m) / 2;
        cv::resize(oImg(cv::Rect(left, top, m, m)), oImg, cv::Size(iImgSize.at(0), iImgSize.at(1)));
        break;
    }
    }
    return RET_OK;
}


char* YOLO_V8::createSession(DL_INIT_PARAM& iParams) {
    // 模型路径合法性检查
    char* Ret = RET_OK;
    std::regex pattern("[\u4e00-\u9fa5]");
    bool result = std::regex_search(iParams.modelPath, pattern);
    if (result)
    {
        Ret = "[YOLO_V8]:Your model path is error.Change your model path without chinese characters.";
        LOG_ERROR("%s", Ret);
        // std::cout << Ret << std::endl;
        return Ret;
    }
    try
    {
        // 参数配置
        rectConfidenceThreshold = iParams.rectConfidenceThreshold;
        iouThreshold = iParams.iouThreshold;
        imgSize = iParams.imgSize;
        modelType = iParams.modelType;
        cudaEnable = iParams.cudaEnable;

        // ORT 环境与会话初始化
        // 注册了自定义日志回调函数
        env = Ort::Env(ORT_LOGGING_LEVEL_VERBOSE, "Yolo", MyLogCallback, nullptr);
        // printf("Ort initializing!\n");
        // env = Ort::Env(ORT_LOGGING_LEVEL_VERBOSE, "Yolo");
        auto providers = Ort::GetAvailableProviders();
        Ort::SessionOptions sessionOption;
        if (iParams.cudaEnable)
        {
            OrtCUDAProviderOptions cudaOption;
            cudaOption.device_id = 0;
            sessionOption.AppendExecutionProvider_CUDA(cudaOption);
        }
        sessionOption.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL); // 开启全部图优化
        sessionOption.SetIntraOpNumThreads(iParams.intraOpNumThreads);  // 设置intra-op并行线程数
        sessionOption.SetLogSeverityLevel(iParams.logSeverityLevel);
        // printf("Ort initialized!\n");

        // 模型加载和会话创建
        const char* modelPath = iParams.modelPath.c_str();
        
        // 加载模型文件并创建会话
        session = new Ort::Session(env, modelPath, sessionOption);
        Ort::AllocatorWithDefaultOptions allocator;     // 内存分配策略
        size_t inputNodesNum = session->GetInputCount();    // 获取输入节点的数量
        // printf("inputNodesNum = %d\n", inputNodesNum);

        // 输入/输出节点信息提取
        for (size_t i = 0; i < inputNodesNum; i++)
        {
            Ort::AllocatedStringPtr input_node_name = session->GetInputNameAllocated(i, allocator);
            char* temp_buf = new char[50];
            strcpy(temp_buf, input_node_name.get());
            inputNodeNames.push_back(temp_buf);
            // printf("ID %d inputNodeName = %s\n", i, temp_buf);
        }
        size_t OutputNodesNum = session->GetOutputCount();
        // printf("OutputNodesNum = %d\n", OutputNodesNum);
        for (size_t i = 0; i < OutputNodesNum; i++)
        {
            Ort::AllocatedStringPtr output_node_name = session->GetOutputNameAllocated(i, allocator);
            char* temp_buf = new char[10];
            strcpy(temp_buf, output_node_name.get());
            outputNodeNames.push_back(temp_buf);
            // printf("ID %d outputNodeName = %s\n", i, temp_buf);
        }
        options = Ort::RunOptions{ nullptr };   // 运行时选项配置
        warmUpSession();    // 预热会话以提升首次推理速度
        return RET_OK;
    }
    catch (const std::exception& e) // 标准异常
    {
        const char* str1 = "[YOLO_V8]:";
        const char* str2 = e.what();
        std::string result = std::string(str1) + std::string(str2);
        char* merged = new char[result.length() + 1];   // 合并错误信息
        std::strcpy(merged, result.c_str());
        LOG_ERROR("%s", merged);
        delete[] merged;
        return "[YOLO_V8]:Create session failed.";
    }
}


// 实现批量推理方法
char* YOLO_V8::runSession(const std::vector<cv::Mat>& imgs, std::vector<std::vector<DL_RESULT>>& batchResults) {
    if (imgs.empty()) {
        return "Empty images array";
    }
    
#ifdef benchmark
    clock_t starttime_1 = clock();
#endif
    
    char* Ret = RET_OK;
    std::vector<cv::Mat> processedImgs;
    processedImgs.reserve(imgs.size());
    
    // 图像预处理
    // 清空之前的缩放比例
    batchResizeScales.clear();
    batchResizeScales.reserve(imgs.size());
    for (size_t i = 0; i < imgs.size(); i++) {
        cv::Mat processedImg;
        float resizeScale;
        preProcess(const_cast<cv::Mat&>(imgs[i]), imgSize, processedImg, resizeScale);
        processedImgs.push_back(std::move(processedImg));
        batchResizeScales.push_back(resizeScale); // 记录当前图像的缩放比例
    }
    
    // 根据模型类型处理
    if (modelType < 4) { // FLOAT32 models
        // 为批次分配内存
        float* blob = new float[imgs.size() * processedImgs[0].total() * 3];
        
        // 转换为blob格式
        blobFromImages(processedImgs, blob);
        
        // 定义输入张量维度 [batchSize, channels, height, width]
        std::vector<int64_t> inputNodeDims = {static_cast<int64_t>(imgs.size()), 3, static_cast<int64_t>(imgSize.at(0)), static_cast<int64_t>(imgSize.at(1))};
        
        // 处理批量张量
        tensorProcess(starttime_1, imgs, blob, inputNodeDims, batchResults);

        delete[] blob;
    }
    else { // FLOAT16 models
#ifdef USE_CUDA
        // 分配半精度内存
        half* blob = new half[imgs.size() * processedImgs[0].total() * 3];
        blobFromImages(processedImgs, blob);
        std::vector<int64_t> inputNodeDims = {static_cast<int64_t>(imgs.size()), 3, static_cast<int64_t>(imgSize.at(0)), static_cast<int64_t>(imgSize.at(1))};
        tensorProcess(starttime_1, imgs, blob, inputNodeDims, batchResults);
        delete[] blob;
#endif
    }
    
    return Ret;
}

/// @brief 执行张量处理全流程（推理+后处理）
/// @tparam N 数据类型模板参数（float或half）
/// @param starttime_1 预处理阶段起始时间戳（用于性能分析）
/// @param iImg 原始输入图像（用于结果坐标映射）
/// @param blob 预处理后的输入张量数据
/// @param inputNodeDims 输入张量维度信息（NCHW格式）
/// @param oResult 输出检测结果容器
/// @return 返回执行状态字符指针
template<typename N>
char* YOLO_V8::tensorProcess(clock_t& starttime_1, const std::vector<cv::Mat>& imgs, 
                                 N& blob, std::vector<int64_t>& inputNodeDims, 
                                 std::vector<std::vector<DL_RESULT>>& batchResults) {
    // 创建ONNX Runtime输入张量
    Ort::Value inputTensor = Ort::Value::CreateTensor<typename std::remove_pointer<N>::type>(
        Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU),
        blob,
        imgs.size() * 3 * imgSize.at(0) * imgSize.at(1),
        inputNodeDims.data(),
        inputNodeDims.size());
    
#ifdef benchmark
    clock_t starttime_2 = clock();
#endif
    
    // 执行模型推理
    auto outputTensors = session->Run(
        options,
        inputNodeNames.data(),
        &inputTensor,
        1,
        outputNodeNames.data(),
        outputNodeNames.size());
    
#ifdef benchmark
    clock_t starttime_3 = clock();
#endif
    
    // 解析输出张量
    Ort::TypeInfo typeInfo = outputTensors.front().GetTypeInfo();
    auto tensor_info = typeInfo.GetTensorTypeAndShapeInfo();
    std::vector<int64_t> outputNodeDims = tensor_info.GetShape();
    auto output = outputTensors.front().GetTensorMutableData<typename std::remove_pointer<N>::type>();
    
    // 初始化每个图像的结果容器
    batchResults.resize(imgs.size());
    
    // 根据模型类型进行不同后处理
    switch (modelType) {
    case YOLO_DETECT_V8:
    case YOLO_DETECT_V8_HALF:
    {
        // 维度解析：[batchSize, 84, 8400]
        int classNum = outputNodeDims[1] - 4; // 减去框坐标
        int signalResultNum = outputNodeDims[1]; // 84 = 4(box) + 80(class)
        int strideNum = outputNodeDims[2]; // 8400 = 3种尺度预测总数
        
        // 处理每个批次的结果
        for (size_t b = 0; b < imgs.size(); b++) {
            const float currentResizeScale = batchResizeScales[b]; // 获取当前图像的缩放比例
            std::vector<int> class_ids;
            std::vector<float> confidences;
            std::vector<cv::Rect> boxes;
            
            // 创建opencv矩阵处理当前批次的输出数据
            cv::Mat rawData;
            if (modelType == YOLO_DETECT_V8) {
                // 从输出数据中提取当前批次的部分
                rawData = cv::Mat(signalResultNum, strideNum, CV_32F, 
                                  (void*)(reinterpret_cast<float*>(output) + b * signalResultNum * strideNum));
            } else {
                // FP16类型转FP32
                rawData = cv::Mat(signalResultNum, strideNum, CV_16F, 
                                  (void*)(reinterpret_cast<half*>(output) + b * signalResultNum * strideNum));
                rawData.convertTo(rawData, CV_32F);
            }
            
            rawData = rawData.t();  // 转置
            
            float* data = (float*)rawData.data;
            // 遍历所有预测框
            for (int i = 0; i < strideNum; ++i) {
                float* classesScores = data + 4;
                
                // 创建概率矩阵并找到最大概率类别
                cv::Mat scores(1, this->classes.size(), CV_32FC1, classesScores);
                cv::Point class_id;
                double maxClassScore;
                cv::minMaxLoc(scores, 0, &maxClassScore, 0, &class_id);
                
                // 置信度过滤
                if (maxClassScore > rectConfidenceThreshold) {
                    // 解析框坐标
                    float x = data[0];
                    float y = data[1];
                    float w = data[2];
                    float h = data[3];
                    
                    // 坐标反算到原始图像尺寸
                    int left = int((x - 0.5 * w) * currentResizeScale); // 使用当前图像的缩放比例
                    int top = int((y - 0.5 * h) * currentResizeScale);
                    int width = int(w * currentResizeScale);
                    int height = int(h * currentResizeScale);
                    
                    // 保存有效结果
                    confidences.push_back(maxClassScore);
                    class_ids.push_back(class_id.x);
                    boxes.push_back(cv::Rect(left, top, width, height));
                }
                data += signalResultNum;
            }
            
            // 非极大值抑制(NMS)过滤重叠框
            std::vector<int> nmsResult;
            cv::dnn::NMSBoxes(boxes, confidences, rectConfidenceThreshold, iouThreshold, nmsResult);
            
            // 最终结果
            for (int i = 0; i < nmsResult.size(); ++i) {
                int idx = nmsResult[i];
                DL_RESULT result;
                result.classId = class_ids[idx];
                result.confidence = confidences[idx];
                result.box = boxes[idx];
                batchResults[b].push_back(result);
            }
        }
        
#ifdef benchmark
        clock_t starttime_4 = clock();
        double pre_process_time = (double)(starttime_2 - starttime_1) / CLOCKS_PER_SEC * 1000;
        double process_time = (double)(starttime_3 - starttime_2) / CLOCKS_PER_SEC * 1000;
        double post_process_time = (double)(starttime_4 - starttime_3) / CLOCKS_PER_SEC * 1000;
        
        if (cudaEnable) {
            LOG_INFO("[YOLO_V8(CUDA) Batch]: %lfms pre-process, %lfms inference, %lfms post-process for %zu images.", 
                    pre_process_time, process_time, post_process_time, imgs.size());
        } else {
            LOG_INFO("[YOLO_V8(CPU) Batch]: %lfms pre-process, %lfms inference, %lfms post-process for %zu images.", 
                    pre_process_time, process_time, post_process_time, imgs.size());
        }
#endif
        break;
    }
    case YOLO_CLS:
    case YOLO_CLS_HALF:
    {
        // 处理分类模型输出
        for (size_t b = 0; b < imgs.size(); b++) {
            cv::Mat rawData;
            if (modelType == YOLO_CLS) {
                rawData = cv::Mat(1, this->classes.size(), CV_32F, 
                                 (void*)(reinterpret_cast<float*>(output) + b * this->classes.size()));
            } else {
                rawData = cv::Mat(1, this->classes.size(), CV_16F, 
                                 (void*)(reinterpret_cast<half*>(output) + b * this->classes.size()));
                rawData.convertTo(rawData, CV_32F);
            }
            
            float *data = (float*)rawData.data;
            
            // 遍历所有类别概率
            for (int i = 0; i < this->classes.size(); i++) {
                DL_RESULT result;
                result.classId = i;
                result.confidence = data[i];
                batchResults[b].push_back(result);
            }
        }
        break;
    }
    default:
        LOG_ERROR("[YOLO_V8 Batch]: Not supported model type.");
    }
    
    return RET_OK;
}

/// @brief 执行模型热身推理（初始化CUDA/cuDNN上下文，避免首次推理延迟）
/// @return 返回执行状态字符指针（RET_OK表示成功）
// 修复 YOLO.cpp 中的 warmUpSession 函数

char* YOLO_V8::warmUpSession() {
    clock_t starttime_1 = clock();
    // 创建测试图像 -----------------------------------------------------------
    // 生成全零（黑色）的测试图像，尺寸为模型输入要求（如640x640）
    cv::Mat iImg = cv::Mat(cv::Size(imgSize.at(0), imgSize.at(1)), CV_8UC3, cv::Scalar(0, 0, 0));
    cv::Mat processedImg;
    // 执行预处理（与正式推理流程一致）-----------------------------------------
    float warmupResizeScale = 1.0f;
    preProcess(iImg, imgSize, processedImg, warmupResizeScale);
    
    // 创建单图像的向量用于批处理接口
    std::vector<cv::Mat> inputImgs = {processedImg};
    
    // 根据模型类型分支处理 ---------------------------------------------------
    if (modelType < 4)
    {
        // 分配输入数据内存（通道数×宽×高）
        float* blob = new float[processedImg.total() * 3];
        // 生成模型输入Blob（格式转换：HWC->CHW，BGR->RGB，归一化等）
        blobFromImages(inputImgs, blob);
        // 定义输入张量维度（NCHW格式）
        std::vector<int64_t> YOLO_input_node_dims = { 1, 3, static_cast<int64_t>(imgSize.at(0)), static_cast<int64_t>(imgSize.at(1)) };
        // 创建ONNX Runtime输入张量（内存映射方式）
        Ort::Value input_tensor = Ort::Value::CreateTensor<float>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), blob, 3 * imgSize.at(0) * imgSize.at(1),
            YOLO_input_node_dims.data(), YOLO_input_node_dims.size());
        // 执行推理（触发CUDA上下文初始化）-------------------------------------
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(),
            outputNodeNames.size());
        delete[] blob;

        // 计算并输出热身耗时 -------------------------------------------------
        clock_t starttime_4 = clock();
        double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
        if (cudaEnable)
        {   
            LOG_INFO("[YOLO_V8(CUDA)]: Cuda warm-up cost %lfms.", post_process_time);
        }
    }
    else
    {
#ifdef USE_CUDA
        // 分配半精度内存（half类型通常为2字节）
        half* blob = new half[processedImg.total() * 3];
        blobFromImages(inputImgs, blob);
        // 输入维度与浮点版本一致
        std::vector<int64_t> YOLO_input_node_dims = { 1, 3, static_cast<int64_t>(imgSize.at(0)), static_cast<int64_t>(imgSize.at(1)) };
        // 创建FP16输入张量
        Ort::Value input_tensor = Ort::Value::CreateTensor<half>(
            Ort::MemoryInfo::CreateCpu(OrtDeviceAllocator, OrtMemTypeCPU), 
            blob, 
            3 * imgSize.at(0) * imgSize.at(1), 
            YOLO_input_node_dims.data(), 
            YOLO_input_node_dims.size());
        // 执行推理（初始化CUDA流和显存分配）
        auto output_tensors = session->Run(options, inputNodeNames.data(), &input_tensor, 1, outputNodeNames.data(), outputNodeNames.size());
        delete[] blob;

        // 耗时计算与输出
        clock_t starttime_4 = clock();
        double post_process_time = (double)(starttime_4 - starttime_1) / CLOCKS_PER_SEC * 1000;
        if (cudaEnable)
        {   
            LOG_INFO("[YOLO_V8(CUDA)]: Cuda warm-up cost %lfms. ", post_process_time);
        }
#endif
    }
    return RET_OK;
}

// 新增批量检测函数
bool YOLO_V8::detect(std::vector<inferenceTask>& tasks) {
    // 确保批量大小有效
    if (tasks.empty()) {
        LOG_ERROR("Invalid batch parameters: empty tasks or non-positive batch size");
        return false;
    }
    
    // 实际处理的批量大小不超过任务数量
    int actualBatchSize = tasks.size();
    
    // ============== 1. 图像解码 ==============
    std::vector<cv::Mat> images;
    images.reserve(actualBatchSize);

    // 任务ID到索引的映射，用于后续关联结果
    std::unordered_map<int, int> taskIndexMap;
    int validTaskCount = 0;
    
    for (int i = 0; i < actualBatchSize; ++i) {
        inferenceTask& task = tasks[i];
        const uchar* p_data = reinterpret_cast<const uchar*>(task.imgData.data());
        int data_length = static_cast<int>(task.imgData.size());
        cv::Mat encoded_img(1, data_length, CV_8UC1, (void*)p_data);
        cv::Mat img = cv::imdecode(encoded_img, cv::IMREAD_COLOR);
        
        if (img.empty()) {
            LOG_ERROR("Failed to decode image at index %d", i);
            continue;
        }
        
        // 记录任务索引
        taskIndexMap[validTaskCount] = i;
        images.push_back(img);
        validTaskCount++;
    }
    
    // 如果没有成功解码的图像，提前返回
    if (images.empty()) {
        LOG_ERROR("No valid images decoded for batch processing");
        return false;
    }
    
    // ============== 2. 执行批量目标检测 ==============
    std::vector<std::vector<DL_RESULT>> batchResults;
    runSession(images, batchResults);
    
    // ============== 3. 处理每个任务的结果(先不处理错误的情况) ==============
    
    for (int i = 0; i < batchResults.size(); ++i) {
        // 获取原始任务索引
        int taskIdx = taskIndexMap[i];
        inferenceTask& task = tasks[taskIdx];
        
        const auto& res = batchResults[i];
        
        // 统计检测结果
        std::unordered_map<std::string, int> actual_counts;
        for (const auto& cls : classes) {
            actual_counts[cls] = 0;
        }
        for (const auto& re : res) {
            actual_counts[classes[re.classId]]++;
        }
        
        LOG_INFO("Detection summary for task %d:", task.clientId);
        for (const auto& cls : classes) {
            LOG_INFO("[%s] Expected: %d | Actual: %d", cls.c_str(), expected_counts.at(cls), actual_counts[cls]);
        }
        
        // 筛选目标骨骼
        bool success = parseResult(res, task.bbox);
        
    }
    
    return true;
}

bool YOLO_V8::detect(const std::vector<cv::Mat>& images, std::vector<std::unordered_map<std::string, cv::Rect>>& selected_boxes) {
    int batchSize = images.size();

    std::vector<std::vector<DL_RESULT>> batchResults;
    runSession(images, batchResults);

    // 统计检测结果
    std::vector<int> resultCounts(batchSize);   // 每张图片的预测框数量
    for(int i = 0; i < batchSize; ++i) {
        resultCounts[i] = batchResults[i].size();
    }

    LOG_INFO("Detect Batch Size: %d", batchSize);

    for(int i = 0; i < batchSize; ++i) {
        LOG_INFO("[%d] Predict Bbox: %d", i, resultCounts[i]);
    }

    for(int i = 0; i < batchSize; ++i) {
        parseResult(batchResults[i], selected_boxes[i]);
    }
    return true;
}


// bool YOLO_V8::detect(inferenceTask& task, std::unordered_map<std::string, cv::Rect>& selected_boxes) {
//     // ============== 1. 图像解码 ==============
//     const uchar* p_data = reinterpret_cast<const uchar*>(task.imgData.data());
//     int data_length = static_cast<int>(task.imgData.size());
//     cv::Mat encoded_img(1, data_length, CV_8UC1, (void*)p_data);
//     cv::Mat img = cv::imdecode(encoded_img, cv::IMREAD_COLOR);
//     if (img.empty()) {
//         LOG_ERROR("Failed to decode image from memory data!");
//         return false;
//     }

//     // ============== 2. 执行目标检测 ==============
//     std::vector<DL_RESULT> res;
//     runSession(img, res);

//     // ============== 3. 统计检测结果 ==============
//     std::unordered_map<std::string, int> actual_counts;
//     for (const auto& cls : classes) {
//         actual_counts[cls] = 0;
//     }
//     for (const auto& re : res) {
//         actual_counts[classes[re.classId]]++;
//     }
//     LOG_INFO("Detection summary:");
//     for (const auto& cls : classes) {
//         LOG_INFO("[%s] Expected: %d | Actual: %d", cls.c_str(), expected_counts.at(cls), actual_counts[cls]);
//     }
    
//     // ============== 5. 筛选目标骨骼 ==============
//     bool ret = parseResult(res, selected_boxes);
//     // ============== 6. 可视化 ==============
//     // YOLO_V8::drawBoxes(img, selected_boxes);
//     // ============== 6. 返回结果 ==============
//     return ret ? true : false;
//     // YOLO_V8::drawBoxes(img, res);
//     // ============== 7. 保存结果 ==============
//     // static int s_image_count = 1;
//     // std::string output_path = "./output/" + std::to_string(s_image_count++) + ".png";
//     // cv::imwrite(output_path, img);
//     // LOG_INFO("Saved result to: %s", output_path.c_str());
// }

    // // 颜色映射（使用高亮颜色）
    // static const std::vector<cv::Scalar> predefined_colors = {
    //     cv::Scalar(0, 255, 255),   // 亮黄 (BGR)
    //     cv::Scalar(255, 255, 0),   // 青色
    //     cv::Scalar(255, 0, 255),   // 品红
    //     cv::Scalar(0, 165, 255),   // 橙色
    //     cv::Scalar(0, 0, 255),     // 纯红
    //     cv::Scalar(255, 0, 0),     // 纯蓝
    //     cv::Scalar(147, 20, 255),  // 深粉
    //     cv::Scalar(203, 192, 255), // 浅粉
    //     cv::Scalar(0, 255, 0),     // 纯绿
    //     cv::Scalar(255, 255, 255), // 纯白
    //     cv::Scalar(180, 105, 255), // 紫红
    //     cv::Scalar(211, 0, 148),   // 玫红
    //     cv::Scalar(224, 176, 255)  // 淡紫
    // };

void YOLO_V8::drawBoxes(cv::Mat& img, std::unordered_map<std::string, cv::Rect>& boxes) {

    // 品红
    const cv::Scalar color = cv::Scalar(255, 255, 0);
    // ============== 可视化绘制（仅绘制选中部分）============== 
    for (const auto& [label_name, box] : boxes) {

        // 绘制边界框（线宽设置在这里->第三个参数）
        cv::rectangle(img, box, color, 2); // 线宽=2像素

        // 构造标签文本
        std::string label = label_name;
        
        // 绘制标签背景
        cv::rectangle(
            img,
            cv::Point(box.x, box.y - 25),
            cv::Point(box.x + label.length() * 15, box.y),
            color,
            cv::FILLED // 填充模式
        );

        // 绘制标签文字（加粗字体）
        cv::putText(
            img,
            label,
            cv::Point(box.x + 2, box.y - 5), // 微调位置避免边缘切割
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,    // 字体比例
            cv::Scalar(0, 0, 0), // 颜色
            2       // 2像素线宽
        );
    }
}

void YOLO_V8::drawBoxes(cv::Mat& img, std::vector<DL_RESULT>& objs) {
    // 品红
    const cv::Scalar color = cv::Scalar(255, 0, 255);
    // ============== 可视化绘制（仅绘制选中部分）============== 
    for (const auto& obj : objs) {

        // 绘制边界框（线宽设置在这里->第三个参数）
        cv::rectangle(img, obj.box, color, 2); // 线宽=2像素

        // 构造标签文本
        std::string label = YOLO_V8::classes[obj.classId];
        
        // 绘制标签背景
        cv::rectangle(
            img,
            cv::Point(obj.box.x, obj.box.y - 25),
            cv::Point(obj.box.x + label.length() * 15, obj.box.y),
            color,
            cv::FILLED // 填充模式
        );

        // 绘制标签文字（加粗字体）
        cv::putText(
            img,
            label,
            cv::Point(obj.box.x + 2, obj.box.y - 5), // 微调位置避免边缘切割
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,    // 字体比例
            cv::Scalar(0, 0, 0), // 颜色
            2       // 2像素线宽
        );
    }
}

// ============== 筛选关键骨骼结构 ==============
// 
// bool YOLO_V8::parseResult(std::vector<DL_RESULT>& objs, std::unordered_map<std::string, cv::Rect>& selected_boxes) {
//         std::map<int, std::vector<cv::Rect>> classifier;
//         for (const auto& re : objs) {
//             classifier[re.classId].push_back(re.box);
//         }

//         // 验证基础骨骼数量
//         if (classifier[0].size() != 1 || classifier[1].size() != 1 || classifier[2].size() != 1) {
//             LOG_ERROR("Base bone detection failed");
//             return false;
//         }
//         if (classifier[3].size() !=4 || classifier[4].size()!=5 || 
//             classifier[5].size()!=4 || classifier[6].size()!=5) {
//             LOG_ERROR("Phalanx count mismatch");
//             return false;
//         }

//         // 基础骨骼
//         selected_boxes["Radius"] = classifier[0][0];
//         selected_boxes["Ulna"] = classifier[1][0];
//         selected_boxes["MCPFirst"] = classifier[2][0];

//         // MCP选择
//         auto& mcp = classifier[3];
//         std::sort(mcp.begin(), mcp.end(), [](const cv::Rect& a, const cv::Rect& b) {
//             return a.x > b.x; // 按x坐标从大到小排序
//         });
//         selected_boxes["MCPThird"] = mcp[1];  // 第三掌骨
//         selected_boxes["MCPFifth"] = mcp[3];  // 第五掌骨

//         // PIP选择
//         auto& pip = classifier[4];
//         std::sort(pip.begin(), pip.end(), [](const cv::Rect& a, const cv::Rect& b) {
//             return a.x > b.x;
//         });
//         selected_boxes["PIPFirst"] = pip[0];
//         selected_boxes["PIPThird"] = pip[2];
//         selected_boxes["PIPFifth"] = pip[4];

//         // MIP选择
//         auto& mip = classifier[5];
//         std::sort(mip.begin(), mip.end(), [](const cv::Rect& a, const cv::Rect& b) {
//             return a.x > b.x;
//         });
//         selected_boxes["MIPThird"] = mip[1];
//         selected_boxes["MIPFifth"] = mip[3];

//         // DIP选择
//         auto& dip = classifier[6];
//         std::sort(dip.begin(), dip.end(), [](const cv::Rect& a, const cv::Rect& b) {
//             return a.x > b.x;
//         });
//         selected_boxes["DIPFirst"] = dip[0];
//         selected_boxes["DIPThird"] = dip[2];
//         selected_boxes["DIPFifth"] = dip[4];

//         return true;
// }

bool YOLO_V8::parseResult(const std::vector<DL_RESULT>& objs, std::unordered_map<std::string, cv::Rect>& selected_boxes) {
    std::map<int, std::vector<cv::Rect>> classifier;
    bool success = true;  // 记录是否至少有一个有效处理

    // 分类检测结果
    for (const auto& re : objs) {
        classifier[re.classId].push_back(re.box);
    }

    // 骨骼基础检测（独立验证每个类别）
    auto process_category = [&](int class_id, std::string prefix, size_t expected_size) {
        auto it = classifier.find(class_id);
        if (it != classifier.end()) {
            if(it->second.size() == expected_size) {
                selected_boxes[prefix] = it->second[0];
                return true;
            }
            else {
                for(size_t i = 0; i < it->second.size(); ++i) {
                    std::string name = prefix + std::to_string(i + 1);
                    selected_boxes[name] = it->second[i];
                }
                LOG_ERROR("%s count mismatch: expected %zu got %zu", 
                        prefix, expected_size, it->second.size());

                return false;
            }
        }
        return false;
    };

    // 处理单实例骨骼（Radius/Ulna/MCPFirst）
    success &= process_category(0, classes[0], 1);
    success &= process_category(1, classes[1], 1);
    success &= process_category(2, classes[2], 1);

    // 处理多实例骨骼（带排序逻辑）
    auto process_multi_joint = [&](int class_id, std::string prefix, 
                                  size_t expected_size, 
                                  const std::vector<int> indices) {
        auto it = classifier.find(class_id);
        
        if (it != classifier.end()) {
            size_t size = it->second.size();
            // 按x坐标降序排序
            std::sort(it->second.begin(), it->second.end(),
                     [](const cv::Rect& a, const cv::Rect& b) { return a.x > b.x; });
            
            if(size == expected_size) {
                // 存储指定位置的骨骼
                for (size_t i = 0; i < indices.size(); ++i) {
                    int idx_ = indices[i] + 1;
                    if(class_id == 3 || class_id == 5) idx_ ++;
                    const std::string name = prefix + std::to_string(idx_);
                    selected_boxes[name] = it->second[indices[i]];
                }
                return true;
            }
            else {
                // 全都存了
                for (size_t i = 0; i < size; ++i) {
                    int idx_ = indices[i] + 1;
                    const std::string name = prefix + std::to_string(idx_);
                    selected_boxes[name] = it->second[i];
                }
                LOG_ERROR("%s count mismatch: expected %zu got %zu", 
                         prefix, expected_size, it->second.size());
                return false;
            }
        }
        return false;
    };

    // 处理各关节类型
    success &= process_multi_joint(3, classes[3], 4, {1, 3});    // 第3、5掌骨
    success &= process_multi_joint(4, classes[4], 5, {0, 2, 4}); // 第1、3、5指节
    success &= process_multi_joint(5, classes[5], 4, {1, 3});    // 第3、5中节
    success &= process_multi_joint(6, classes[6], 5, {0, 2, 4});// 第1、3、5末节

    return success;
}

neb::CJsonObject YOLO_V8::convertToJson(const std::unordered_map<std::string, cv::Rect>& selected_boxes) {
    neb::CJsonObject oResult;  // 默认构造空JSON对象

    for (const auto& [bone_name, rect] : selected_boxes) {
        neb::CJsonObject oBone;  // 子对象

        // 添加坐标和尺寸（直接使用Add方法）
        oBone.Add("x", rect.x);        // int自动转为JSON Number
        oBone.Add("y", rect.y);
        oBone.Add("width", rect.width);
        oBone.Add("height", rect.height);

        // 将子对象添加到根对象
        oResult.Add(bone_name, oBone);
    }

    return oResult;
}

