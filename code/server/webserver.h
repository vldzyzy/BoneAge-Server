#pragma once
#include <unordered_map>
#include <fcntl.h>  // fcntl()设置文件描述符属性
#include <unistd.h> // close() 关闭文件描述符
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h" 

// WebServer类负责实现一个高性能HTTP服务器，主要功能包括：
// 1. 初始化监听socket并设置相关选项；
// 2. 使用epoll进行事件监控；
// 3. 使用线程池处理读写请求；
// 4. 使用堆定时器管理连接超时；
// 5. 使用SQL连接池处理数据库操作。
class WebServer {
public:
    /**
     * 构造函数
     * @param port           服务器监听端口
     * @param trigMode       epoll触发模式（LT/ET组合，0~3对应不同组合）
     * @param timeoutMS      连接超时时间（毫秒）
     * @param optLinger      是否启用优雅关闭（SO_LINGER选项）
     * @param sqlPort        数据库端口
     * @param sqlUser        数据库用户名
     * @param sqlPwd         数据库密码
     * @param dbName         数据库名
     * @param connPoolNum    数据库连接池中连接的数量
     * @param threadNum      线程池中线程的数量
     * @param openLog        是否开启日志
     * @param logLevel       日志等级
     * @param logQueSize     日志队列大小
     */
    WebServer(
        int port, int trigMode, int timeoutMS, bool optLinger, 
        const char* sqlHost, int sqlPort, const char* sqlUser, const  char* sqlPwd, 
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);

    // 析构函数，负责释放资源，如关闭socket、释放内存、关闭连接池等
    ~WebServer();

    // 启动服务器，进入主循环等待和处理事件
    void start();

private:
    // 初始化监听socket（包括创建socket、设置选项、绑定、监听等）
    bool _initSocket(); 

    // 根据传入的触发模式参数设置监听socket和连接socket的事件模式
    void _initEventMode(int trigMode);

    // 添加新客户端连接，创建对应的HttpConn对象，并加入epoll事件监控
    void _addClient(int fd, sockaddr_in addr);
  
    // 处理监听socket上的新连接事件
    void _dealListen();
    // 处理客户端socket上的写事件
    void _dealWrite(HttpConn* client);
    // 处理客户端socket上的读事件
    void _dealRead(HttpConn* client);

    // 发送错误信息给客户端，然后关闭该连接
    void _sendError(int fd, const char* info);
    // 延长客户端连接超时（更新定时器）
    void _extentTime(HttpConn* client);
    // 关闭一个客户端连接（从epoll中删除并释放资源）
    void _closeConn(HttpConn* client);

    // 读事件处理回调：读取客户端数据并进行后续处理
    void _onRead(HttpConn* client);
    // 写事件处理回调：将处理结果写回客户端
    void _onWrite(HttpConn* client);
    // 处理HTTP请求（调用HttpConn::process），根据处理结果设置下一步关注的事件（读/写）
    void onProcess(HttpConn* client);

    // 最大支持的文件描述符数（即最大并发连接数）
    static const int MAX_FD = 65536;

    // 将文件描述符设置为非阻塞模式
    static int setFdNonblock(int fd);

    int _port;            // 服务器监听端口
    bool _openLinger;     // 是否启用优雅关闭（SO_LINGER选项）
    int _timeoutMS;       // 客户端连接超时时间（毫秒）
    bool _isClose;        // 服务器关闭标志
    int _listenFd;        // 监听socket的文件描述符
    char* _srcDir;        // 存放资源的根目录路径
    
    uint32_t _listenEvent;  // 监听socket的事件类型（如EPOLLIN, EPOLLET等）
    uint32_t _connEvent;    // 客户端连接socket的事件类型
    
    // 定时器，用于管理连接超时
    std::unique_ptr<HeapTimer> _timer;
    // 线程池，用于异步处理读写任务
    std::unique_ptr<ThreadPool> _threadpool;
    // 封装epoll操作的类，管理事件监听和分发
    std::unique_ptr<Epoller> _epoller;
    // 记录所有活跃的HTTP连接，key为文件描述符
    std::unordered_map<int, HttpConn> _users;
};