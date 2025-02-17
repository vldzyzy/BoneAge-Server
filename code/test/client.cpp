#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int main() {
    int port = 7413; // 服务器端口
    const char* serverIp = "182.45.244.122"; // 服务器IP地址

    // 创建客户端 socket
    int clientSock = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSock < 0) {
        std::cerr << "创建客户端 socket 失败！" << std::endl;
        return 1;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
        std::cerr << "无效的服务器地址！" << std::endl;
        close(clientSock);
        return 1;
    }

    // 连接服务器
    if (connect(clientSock, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
        std::cerr << "连接服务器失败！" << std::endl;
        close(clientSock);
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

    return 0;
}
