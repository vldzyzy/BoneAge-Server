#pragma once

#include <vector>
#include <opencv2/opencv.hpp>
#include <nn/detect.h>

namespace utils {

void DrawBoxes(cv::Mat& image, const std::vector<nn::DetectionResult>& results);

}