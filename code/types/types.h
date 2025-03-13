#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <future>

#include <opencv4/opencv2/opencv.hpp>

struct PostData {
    std::string _body;
    std::unordered_map<std::string, std::string> field;     // 账号密码什么的小东西
    std::string_view image;
    std::string_view bboxJson;
};


struct inferenceTask {
    int clientId;
    std::shared_ptr<PostData> postData;
    std::string_view imgData;
    // cv::Mat image;          // 通过指针直接构造OpenCV矩阵
    std::string status;
    std::unordered_map<std::string, cv::Rect> bbox;
    std::promise<std::string> resultPromise;

    explicit inferenceTask(int clientId, std::shared_ptr<PostData> pptr)
        : clientId(clientId)
        , postData(std::move(pptr)) 
    {   
        imgData = postData->image;
        // 零拷贝构造 cv::Mat（假设图像格式为 RGB）
        // image = cv::Mat(rows, cols, CV_8UC3, const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(postData->image.data())));
    }
    
    inferenceTask() : clientId(0), postData(nullptr) {}
};