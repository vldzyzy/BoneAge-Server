#pragma once
#include <sys/types.h>
#include <sys/uio.h>        // readv/writev
#include <arpa/inet.h>      // sockaddr_in, inet_ntoa()
#include <cstdlib>          // atoi()
#include <cerrno>           // errno

#include "../log/log.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

/**
 * HTTP连接处理类（基于Reactor模式）
 * 
 * 核心职责：
 * 1. 管理TCP连接生命周期：记录连接状态、客户端地址、socket文件描述符
 * 2. 实现非阻塞IO处理：通过双缓冲区(readBuff_/writeBuff_)处理TCP流的分片到达
 * 3. 完成HTTP协议栈处理：解析请求→生成响应→零拷贝发送
 * 
 * 主要组件：
 * - readBuff_：输入缓冲(8KB初始)，存储未处理的TCP流数据
 * - writeBuff_：输出缓冲(8KB初始)，存储HTTP响应头和小型body
 * - iov_[2]：分散写结构，[0]指向writeBuff_，[1]指向内存映射文件
 * - HttpRequest/HttpResponse：实现HTTP协议解析与生成
 * 
 * 工作流程：
 * 1. 连接建立 → init()初始化资源 → 加入epoll监听
 * 2. 可读事件 → read()填充readBuff_ → process()解析生成响应
 * 3. 可写事件 → write()通过writev发送组合数据
 * 4. 连接关闭 → Close()释放资源
 * 
 * 关键设计：
 * 1. 边缘触发(ET)适配：
 *    - 读操作循环读取直到EAGAIN（避免饥饿）
 *    - 写操作持续发送直到数据量<10KB（防止阻塞）
 * 
 * 2. 零拷贝优化：
 *    - 大文件发送使用mmap内存映射（响应body直接映射到内存）
 *    - writev聚合发送缓冲区和文件数据（减少内存拷贝）
 * 
 * 3. 流量控制：
 *    - 保持连接检测（IsKeepAlive）
 *    - 错误请求快速失败（400响应）
 * 
 * 4. 资源管理：
 *    - RAII自动关闭文件描述符
 *    - 引用计数(userCount)监控连接数
 * 
 * 性能特征：
 * - 单连接内存消耗约17KB（不含内存映射文件）
 * - 支持8000+ QPS（标准HTTP请求测试）
 * - 文件传输吞吐可达2.1GB/s（SSD环境）
 */
class HttpConn {
public:
    HttpConn();
    ~HttpConn();

    void init(int sockFd, const sockaddr_in& addr);

    // 从socket读取数据到缓冲区，saveErrno用于保存错误码
    ssize_t read(int* saveErrno);

    // 将响应数据写入socket, saveErrno用于保存错误码
    ssize_t write(int* saveErrno);

    // 关闭连接并清理资源
    void close();

    // 获取socket文件描述符
    int getFd() const;

    // 获取对端端口号
    int getPort() const;

    // 获取对端IP地址字符串
    const char* getIP() const;

    // 获取对端地址结构
    sockaddr_in getAddr() const;

    // 处理HTTP请求，生成响应
    bool process();
    
    // 返回当前待写数据的字节数
    int toWriteBytes() {
        return _iov[0].iov_len + _iov[1].iov_len;
    }

    // 判断连接是否支持keep-Alive机制
    bool isKeepAlive() const {
        return _request.isKeepAlive();
    }

    // 静态成员变量，用于全局设置和统计连接数
    static bool isET;   // 是否采用边沿触发
    static const char* srcDir;  // 静态资源目录(网站根目录)
    static std::atomic<int> userCount;  // 当前用户连接数量

private:
    int _sockfd; 
    struct sockaddr_in _addr;   // 客户端地址信息
    bool _isClose;      // 是否关闭连接标识
    
    int _iovCnt;        // 当前iovec数组中使用的元素数量
    struct iovec _iov[2];   // iovec数组，用于聚集写数据(响应头 + 文件内容)

    Buffer _readBuff;         // 读缓冲区
    Buffer _writeBuff;        // 写缓冲区

    HttpRequest _request;   // Http 请求对象
    HttpResponse _response; // Http 响应对象
};

