#include "server/webserver.h"
#include <thread>
#include <chrono>
#include <iostream>

int main() {
    // 配置服务器参数
    int port = 80;              // 监听端口
    int trigMode = 3;             // 触发模式：监听和连接均使用 ET 模式
    int timeoutMS = 600000;         // 连接超时 60 s
    bool optLinger = false;       // 不启用 SO_LINGER 优雅关闭
    const char* sqlHost = "172.17.0.1";  // 数据库host
    int sqlPort = 3306;           // 数据库端口（根据实际情况设置）
    const char* sqlUser = "webserver"; // 数据库用户名
    const char* sqlPwd = "ZYzy@2025+-*"; // 数据库密码
    const char* dbName = "webserver";  // 数据库名称
    int connPoolNum = 8;          // SQL 连接池数量
    int threadNum = 8;            // 线程池中线程数量
    bool openLog = true;          // 开启日志
    int logLevel = 0;             // 日志等级
    int logQueSize = 1024;        // 日志队列大小
    const char* modelPath = "models"; // onnx模型路径

    if (mysql_library_init(0, nullptr, nullptr)) {
        std::cerr << "Could not initialize MySQL library" << std::endl;
        return 1;
    }

    // 创建 WebServer 实例
    WebServer server(port, trigMode, timeoutMS, optLinger,
                     sqlHost, sqlPort, sqlUser, sqlPwd, dbName, connPoolNum,
                     threadNum, openLog, logLevel, logQueSize, modelPath);

    // 在子线程中启动服务器（start() 会进入事件循环）
    std::thread serverThread([&server]() {
        server.start();
    });

    // 创建推理线程
    std::thread inferenceThread(inference);

    // 等待服务器启动完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 等待服务器运行
    inferenceThread.join();
    serverThread.join();

    return 0;
}
