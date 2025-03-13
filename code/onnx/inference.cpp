#include "inference.h"
#include "YOLO.h"
#include "BoneAgeCls.h"

BlockQueue<inferenceTask> inferenceQueue(100);

void inference() {
    // 确保模型已初始化
    // detector->init("path/to/model.onnx");
    
    // 配置参数
    const int MAX_BATCH_SIZE = 8;  // 最大批处理大小
    
    LOG_INFO("Inference thread started");
    
    while (true) {
        // 使用vector存储推理任务（值对象，不是指针）
        std::vector<inferenceTask> taskBatch;
        taskBatch.reserve(MAX_BATCH_SIZE);
        
        // 1. 从队列获取第一个任务
        inferenceTask task; // 创建临时对象接收队列元素
        if (!inferenceQueue.pop_move(task)) {
            // 队列已关闭，退出线程
            LOG_INFO("Inference queue closed, exiting thread");
            break;
        }
        
        // 使用移动语义将任务添加到批处理中
        taskBatch.push_back(std::move(task));
        
        // 2. 尝试非阻塞方式获取更多任务填满批次
        for (int i = 1; i < MAX_BATCH_SIZE && !inferenceQueue.empty(); ++i) {
            inferenceTask nextTask;
            if (inferenceQueue.pop_move(nextTask, 0)) { // 非阻塞获取，超时为0
                taskBatch.push_back(std::move(nextTask));
            } else {
                break;
            }
        }
        
        // 3. 处理批次
        LOG_INFO("Processing batch of %zu tasks", taskBatch.size());
        
        try {
            size_t batchSize = taskBatch.size();
            // 构造图像
            std::vector<cv::Mat> images(batchSize);
            for(int i = 0; i < batchSize; ++i) {
                const uchar* p_data = reinterpret_cast<const uchar*>(taskBatch[i].imgData.data());
                int data_length = static_cast<int>(taskBatch[i].imgData.size());
                cv::Mat encoded_img(1, data_length, CV_8UC1, (void*)p_data);
                cv::Mat original_img = cv::imdecode(encoded_img, cv::IMREAD_UNCHANGED);

                // 转换为三通道 RGB 图像
                int channels = original_img.channels();
                cv::Mat processed_img;
                if (channels == 1) {
                    // 单通道图像：转换为三通道 RGB
                    cv::cvtColor(original_img, processed_img, cv::COLOR_GRAY2RGB);
                    LOG_INFO("接收到单通道图像");
                } 
                else if (channels == 3) {
                    // 三通道图像：从 BGR 转换为 RGB
                    cv::cvtColor(original_img, processed_img, cv::COLOR_BGR2RGB);
                    LOG_INFO("接收到三通道图像");
                }

                LOG_INFO("image size: %d", processed_img.total());

                images[i] = std::move(processed_img);
            }

            std::vector<std::unordered_map<std::string, cv::Rect>> selected_boxes(batchSize);

            // 执行批量推理
            YOLO_V8::instance()->detect(images, selected_boxes);

            std::vector<cv::Mat> cls_images_all;
            std::vector<std::string> cls_categories_all;

            std::vector<cv::Mat> cls_images;
            std::vector<std::string> cls_categories;

            // 事先记录好每个cls推理任务的区域数量，将每个任务在batch中的索引当作id
            std::vector<int> region_counts(batchSize);

            for(int i = 0; i < batchSize; ++i) {
                generate_image_regions(images[i], selected_boxes[i], cls_images, cls_categories);
                region_counts[i] = cls_categories.size();
                cls_images_all.insert(cls_images_all.end(), cls_images.begin(), cls_images.end());
                cls_categories_all.insert(cls_categories_all.end(), cls_categories.begin(), cls_categories.end());
            }

            BoneAgeCls::instance()->predict(cls_images_all, cls_categories_all);
            std::vector<int> clsResults = BoneAgeCls::instance()->getPredictions();

            // 拆分为每个任务, 生成结果
            int offset = 0; // 结果数组的偏移量
            for (int i = 0; i < batchSize; ++i) {
                int count = region_counts[i]; // 当前任务的区域数量
                std::vector<int> taskClsResults; // 当前任务的结果数组

                // 提取当前任务的分类结果
                for (int j = 0; j < count; ++j) {
                    taskClsResults.push_back(clsResults[offset + j]);
                }

                // 构造目标检测和分类任务json结果
                neb::CJsonObject detectJson = YOLO_V8::convertToJson(selected_boxes[i]);

                neb::CJsonObject clsJson;
                for (int i : taskClsResults) {
                    clsJson.Add(i); // 将每个整数添加到 JSON 数组
                }
                    
                // 构造当前任务的 JSON 对象
                neb::CJsonObject taskJson;

                taskJson.Add("detect_result", detectJson);
                taskJson.Add("predict_result", clsJson);

                taskBatch[i].resultPromise.set_value(taskJson.ToString());

                offset += count; // 更新偏移量
            }
        } catch (const std::exception& e) {
            printf("Batch processing error: %s", e.what());
        }
        
        // batch中的对象会在这里自动销毁（离开作用域）
    }
}