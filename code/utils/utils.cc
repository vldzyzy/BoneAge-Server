#include "utils.h"

namespace utils {

void DrawBoxes(cv::Mat& image, const std::vector<nn::DetectionResult>& results) {
    // 品红
    const cv::Scalar color = cv::Scalar(255, 0, 255);
    for (const auto& obj : results) {

        cv::rectangle(image, obj.box, color, 2); // 线宽=2像素

        std::string label = "hh";
        
        cv::rectangle(
            image,
            cv::Point(obj.box.x, obj.box.y - 25),
            cv::Point(obj.box.x + label.length() * 15, obj.box.y),
            color,
            cv::FILLED
        );

        cv::putText(
            image,
            label,
            cv::Point(obj.box.x + 2, obj.box.y - 5),
            cv::FONT_HERSHEY_SIMPLEX,
            0.8,    // 字体比例
            cv::Scalar(0, 0, 0),
            2       // 2像素线宽
        );
    }
}

}