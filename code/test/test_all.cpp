#include "webserver.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    // 配置服务器参数
    int port = 8080;              // 监听端口
    int trigMode = 3;             // 触发模式：监听和连接均使用 ET 模式
    int timeoutMS = 60000;         // 连接超时 60 s
    bool optLinger = false;       // 不启用 SO_LINGER 优雅关闭
    int sqlPort = 3306;           // 数据库端口（根据实际情况设置）
    const char* sqlUser = "root"; // 数据库用户名
    const char* sqlPwd = "ZYzy@2025+-*"; // 数据库密码
    const char* dbName = "webserver";  // 数据库名称
    int connPoolNum = 8;          // SQL 连接池数量
    int threadNum = 4;            // 线程池中线程数量
    bool openLog = true;          // 开启日志
    int logLevel = 1;             // 日志等级
    int logQueSize = 1024;        // 日志队列大小

    // 创建 WebServer 实例
    WebServer server(port, trigMode, timeoutMS, optLinger,
                     sqlPort, sqlUser, sqlPwd, dbName, connPoolNum,
                     threadNum, openLog, logLevel, logQueSize);

    // 在子线程中启动服务器（start() 会进入事件循环）
    std::thread serverThread([&server]() {
        server.start();
    });

    // 等待一段时间，确保服务器启动完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 创建客户端 socket，模拟 HTTP 请求
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        std::cerr << "创建客户端 socket 失败！" << std::endl;
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) <= 0) {
        std::cerr << "无效的服务器地址！" << std::endl;
        return 1;
    }

    // 连接服务器
    if (connect(clientSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "连接服务器失败！" << std::endl;
        return 1;
    }
    std::cout << "已连接到 WebServer" << std::endl;

    // 构造 HTTP GET 请求
    const char* httpRequest = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    if (send(clientSock, httpRequest, strlen(httpRequest), 0) < 0) {
        std::cerr << "发送请求失败！" << std::endl;
        close(clientSock);
        return 1;
    }
    std::cout << "已发送 HTTP 请求：" << httpRequest;

    // 接收服务器返回的数据
    char buffer[4096] = {0};
    int bytesReceived = recv(clientSock, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived < 0) {
        std::cerr << "接收响应失败！" << std::endl;
    } else {
        buffer[bytesReceived] = '\0';
        std::cout << "收到的响应：" << std::endl;
        std::cout << buffer << std::endl;
    }

    // 关闭客户端 socket
    close(clientSock);

    // 为了测试方便，这里等待一会后退出程序（实际项目中可增加优雅退出机制）
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "测试结束，退出程序。" << std::endl;
    // 注意：由于 WebServer 的 start() 是阻塞循环，这里直接调用 exit(0) 强制退出整个进程
    exit(0);

    // 如果有优雅停止服务器的接口，则可调用 server.stop(); 并 join 线程
    // serverThread.join();
    return 0;
}
