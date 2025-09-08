#pragma once

#include <vector>
#include <opencv2/opencv.hpp>
#include <nn/yolo11_detector.h>

namespace utils {

void DrawBoxes(cv::Mat& image, const std::vector<DetectionResult>& results);

}