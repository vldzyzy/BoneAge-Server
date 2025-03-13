#pragma once

#define RET_OK nullptr

#include <string>
#include <vector>
#include <cstdio>
#include <opencv4/opencv2/opencv.hpp>
#include <onnxruntime/onnxruntime_cxx_api.h>
#include <cuda_fp16.h>

#include "../log/log.h"
#include "../json/CJsonObject.hpp"
#include "inference.h"

enum MODEL_TYPE
{
    //FLOAT32 MODEL
    YOLO_DETECT_V8 = 1,
    YOLO_POSE = 2,
    YOLO_CLS = 3,

    //FLOAT16 MODEL
    YOLO_DETECT_V8_HALF = 4,
    YOLO_POSE_V8_HALF = 5,
    YOLO_CLS_HALF = 6
};


typedef struct _DL_INIT_PARAM
{
    std::string modelPath;
    MODEL_TYPE modelType = YOLO_DETECT_V8;
    std::vector<int> imgSize = { 640, 640 };
    float rectConfidenceThreshold = 0.6;
    float iouThreshold = 0.5;
    int	keyPointsNum = 2;//Note:kpt number for pose
    bool cudaEnable = false;
    int logSeverityLevel = 3;
    int intraOpNumThreads = 1;
} DL_INIT_PARAM;


typedef struct _DL_RESULT
{
    int classId;
    float confidence;
    cv::Rect box;
    std::vector<cv::Point2f> keyPoints;
} DL_RESULT;


class YOLO_V8
{
public:
    ~YOLO_V8();
    
    static YOLO_V8* instance();

    void init(const char* modelPath);

    bool detect(const std::vector<cv::Mat>& images, std::vector<std::unordered_map<std::string, cv::Rect>>& selected_boxes_batch);
    bool detect(std::vector<inferenceTask>& tasks);

    static bool parseResult(const std::vector<DL_RESULT>& objs, std::unordered_map<std::string, cv::Rect>& selected_boxes);

    static void drawBoxes(cv::Mat& img, std::unordered_map<std::string, cv::Rect>& boxes);

    static void drawBoxes(cv::Mat& img, std::vector<DL_RESULT>& objs);

    template<typename T>
    char* blobFromImages(const std::vector<cv::Mat>& imgs, T& iBlob);

    char* createSession(DL_INIT_PARAM& iParams);

    char* runSession(const std::vector<cv::Mat>& imgs, std::vector<std::vector<DL_RESULT>>& batchResults);

    char* warmUpSession();

    template<typename N>
    char* tensorProcess(clock_t& starttime_1, const std::vector<cv::Mat>& imgs, 
                                 N& blob, std::vector<int64_t>& inputNodeDims, 
                                 std::vector<std::vector<DL_RESULT>>& batchResults);

    char* preProcess(cv::Mat& iImg, std::vector<int> iImgSize, cv::Mat& oImg, float& oResizeScale);

    inline static const std::vector<std::string> classes = {"Radius", "Ulna", "FMCP", "MCP", "PIP", "MIP", "DIP"};

    // 类别预期数量配置（按需求修改）
    inline static const std::unordered_map<std::string, int> expected_counts = {
        {"Radius", 1},
        {"Ulna",   1},
        {"FMCP",   1},
        {"MCP",    4},
        {"PIP",    5},
        {"MIP",    4},
        {"DIP",    5}
    };

    static neb::CJsonObject convertToJson(const std::unordered_map<std::string, cv::Rect>& selected_boxes);

private:
    YOLO_V8();

    Ort::Env env;
    Ort::Session* session;
    bool cudaEnable;
    Ort::RunOptions options;
    std::vector<const char*> inputNodeNames;
    std::vector<const char*> outputNodeNames;

    MODEL_TYPE modelType;
    std::vector<int> imgSize;
    float rectConfidenceThreshold;
    float iouThreshold;
    std::vector<float> batchResizeScales; // 存储每个图像的缩放比例

    // 新增
    static void ORT_API_CALL MyLogCallback(
        void* param, 
        OrtLoggingLevel severity, 
        const char* category, 
        const char* logid, 
        const char* code_location, 
        const char* message
    );

    static int ConvertOrtLogLevel(OrtLoggingLevel ortLevel);

    
};