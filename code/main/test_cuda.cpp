#include <onnxruntime/onnxruntime_cxx_api.h>
#include <iostream>

int main() {
    try {
        Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "test");
        Ort::SessionOptions sessionOptions;

        // 配置 CUDA 选项
        OrtCUDAProviderOptions cudaOption;
        cudaOption.device_id = 0;
        sessionOptions.AppendExecutionProvider_CUDA(cudaOption);

        // 加载一个空模型（仅用于触发初始化）
        Ort::Session session(env, "test.onnx", sessionOptions);
        std::cout << "CUDA 初始化成功！" << std::endl;
    } catch (const Ort::Exception& e) {
        std::cerr << "CUDA 初始化失败: " << e.what() << std::endl;
    }
    return 0;
}