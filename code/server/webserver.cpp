#include "webserver.h"

using namespace std;

WebServer::WebServer(int port, int trigMode, int timeoutMS, bool optLinger,
                    const char* sqlHost, int sqlPort, const char* sqlUser,
                    const char* sqlPwd, const char* dbName, int connPoolNum,
                    int threadNum, bool openLog, int logLevel, int logQueSize,
                    const char* modelPath)
                : _port(port)
                , _openLinger(optLinger)
                , _timeoutMS(timeoutMS)
                , _isClose(false)
                , _timer(make_unique<HeapTimer>())
                , _threadpool(make_unique<ThreadPool>(threadNum))
                , _epoller(new Epoller())
                {
                    _srcDir = getcwd(nullptr, 256);
                    assert(_srcDir);
                    // 将资源目录附加到当前路径上
                    strncat(_srcDir, "/res/", 16);

                    //初始化HTTP连接类中的静态变量
                    HttpConn::userCount = 0;
                    HttpConn::srcDir = _srcDir;

                    if(openLog) Log::instance()->init(logLevel, "./log", ".log", logQueSize);
                    // 初始化sql连接池（单例）
                    SqlConnPool::instance()->init(sqlHost, sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

                    _timer->add(33060, 1000*60*60*2, std::bind(&WebServer::pingSqlCallback, this));

                    // 初始化onnxruntime
                    // 计算新字符串的长度
                    size_t len1 = std::strlen(modelPath);

                    size_t len2 = len1 + std::strlen("/yolo11m_detect.onnx");
                    size_t len3 = len1 + std::strlen("/BoneMaturityCls.onnx");

                    // 分配内存
                    char* yoloPath = new char[len2];
                    char* clsPath = new char[len3];

                    // 拼接字符串
                    std::strcpy(yoloPath, modelPath); 
                    std::strcpy(clsPath, modelPath);
                                    
                    std::strcat(yoloPath, "/yolo11m_detect.onnx");          // 添加分隔符
                    std::strcat(clsPath, "/BoneMaturityCls.onnx");        // 添加第二个路径

                    YOLO_V8::instance()->init(yoloPath);
                    BoneAgeCls::instance()->init(clsPath);

                    // 根据传入的触发模式设置监听和连接的事件模式(LT, ET)
                    _initEventMode(trigMode);

                    // 初始化监听socket
                    if(!_initSocket()) _isClose = true;


                    // 初始化日志系统
                    if(openLog) {
                        if(_isClose) {
                            LOG_ERROR("========== Server init error! ===========");
                        }
                        else {
                            LOG_INFO("========== Server init ===========");
                            LOG_INFO("Port:%d, openLinger: %s", _port, optLinger ? "true" : "false");
                            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                                    (_listenEvent & EPOLLET ? "ET" : "LT"),
                                    (_connEvent & EPOLLET ? "ET" : "LT"));
                            LOG_INFO("LogSys level: %d", logLevel);
                            LOG_INFO("srcDir: %s", HttpConn::srcDir);
                            LOG_INFO("SqlConnPool num: %d, ThreaPool num: %d", connPoolNum, threadNum);
                        }
                    }

                }

WebServer::~WebServer() {
    close(_listenFd);
    _isClose = true;
    free(_srcDir);
    SqlConnPool::instance()->closePool();
    LOG_INFO("========== Server Shutdown ===========");
}

/**
 * 根据传入的触发模式设置监听socket和连接socket的事件模式
 * trigMode:
 *   0 - 两者均为LT模式
 *   1 - 监听socket LT, 连接socket ET
 *   2 - 监听socket ET, 连接socket LT
 *   3 - 两者均为ET模式
 */
void WebServer::_initEventMode(int trigMode) {
    // EPOLLRDHUP: 检测对端关闭连接的事件
    _listenEvent = EPOLLRDHUP;
    // EPOLLONESHOT: 确保某个事件处理完成之前不会重复触发
    _connEvent = EPOLLONESHOT | EPOLLRDHUP;
    switch(trigMode)
    {
        case 0:
            // LT
            break;
        case 1:
            // 连接socket使用ET
            _connEvent |= EPOLLET;
            break;
        case 2:
            // 监听socket使用ET
            _listenEvent |= EPOLLET;
            break;
        case 3:
            // 均使用ET
            _listenEvent |= EPOLLET;
            _connEvent |= EPOLLET;
        default:
            // 默认均使用ET
            _listenEvent |= EPOLLET;
            _connEvent |= EPOLLET;
            break;
    }
    // 同时更新HTTP连接类的静态变量，方便后续处理
    HttpConn::isET = (_connEvent & EPOLLET);
}

// 主循环
void WebServer::start() {
    int timeMS = -1;
    if(!_isClose) {
        LOG_INFO("========== Server Start ===========");
    }
    // 主事件循环：直到服务器被关闭
    while(!_isClose) {       
        // 如果设置了超时，获取定时器中最近到期的时间
        if(_timeoutMS > 0) timeMS = _timer->getNextTick();

        // 阻塞等待事件发生
        int eventCnt = _epoller->wait(timeMS);
        // 遍历所有就绪事件并进行处理
        for(int i = 0; i < eventCnt; ++i) {
            int fd = _epoller->getEventFd(i);           // 获取触发事件的文件描述符
            uint32_t events = _epoller->getEvents(i);   // 获取事件类型
            if(fd == _listenFd) {
                // 处理新连接
                _dealListen();
            }
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                // 出现错误或对端关闭连接，关闭该客户端连接
                assert(_users.count(fd) > 0);
                _closeConn(&_users[fd]);
            }
            else if(events & EPOLLIN) {
                // 可读事件
                assert(_users.count(fd) > 0);
                _dealRead(&_users[fd]);
            }
            else if(events & EPOLLOUT) {
                // 可写事件
                assert(_users.count(fd) > 0);
                _dealWrite(&_users[fd]);
            }
            else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

/**
 * 发送错误信息给客户端，并关闭该连接
 * @param fd   客户端文件描述符
 * @param info 错误信息字符串
 */
void WebServer::_sendError(int fd, const char* info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

/**
 * 关闭客户端连接：从epoll中删除、调用HttpConn关闭函数、记录日志
 */
void WebServer::_closeConn(HttpConn* client) {
    assert(client);
    // LOG_INFO("Client[%d] quit!", client->getFd());
    _epoller->delFd(client->getFd());
    client->close();
}

/**
 * 添加新客户端连接：
 * 1. 初始化HttpConn对象；
 * 2. 若启用超时，则在定时器中添加该连接；
 * 3. 将该连接加入epoll监控，并设置为非阻塞模式
 */
void WebServer::_addClient(int fd, sockaddr_in addr) {
    assert(fd > 0);
    _users[fd].init(fd, addr);
    if(_timeoutMS > 0) {
        _timer->add(fd, _timeoutMS, std::bind(&WebServer::_closeConn, this, &_users[fd]));
    }
    // 关注读事件和连接自定义事件
    _epoller->addFd(fd, EPOLLIN | _connEvent);
    setFdNonblock(fd);
    // LOG_INFO("Client[%d] in!", _users[fd].getFd());
}

/**
 * 处理监听socket上的新连接：
 * 使用循环不断accept（在ET模式下需要循环直到所有连接都被接受）, 因为用的是非阻塞文件描述符，没有待处理的连接请求，accept会返回-1
 */
void WebServer::_dealListen() {
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(_listenFd, (struct sockaddr*)&addr, &len);
        if(fd <= 0) return;
        else if(HttpConn::userCount >= MAX_FD) {
            // 达到最大连接数后，告知客户端服务器繁忙
            _sendError(fd, "Server busy!");
            LOG_WARN("Client is full!");
            return;
        }
        _addClient(fd, addr);
    } while(_listenEvent & EPOLLET);
}

/** 
 * 处理客户端读事件：
 * 延长该连接的超时时间，并将读任务提交给线程池处理
 */
void WebServer::_dealRead(HttpConn* client) {
    assert(client);
    _extentTime(client);
    _threadpool->addTask(std::bind(&WebServer::_onRead, this, client));
}

/**
 * 处理客户端写事件：
 * 延长该连接的超时时间，并将写任务提交给线程池处理
 */
void WebServer::_dealWrite(HttpConn* client) {
    assert(client);
    _extentTime(client);
    _threadpool->addTask(std::bind(&WebServer::_onWrite, this, client));
}

/**
 * 延长客户端连接的超时时间（重置定时器）
 */
void WebServer::_extentTime(HttpConn* client) {
    assert(client);
    if(_timeoutMS > 0) {
        _timer->adjust(client->getFd(), _timeoutMS);
    }
}

/**
 * 读事件任务回调：
 * 尝试从客户端读取数据，若失败则关闭连接，否则调用OnProcess处理请求
 */
void WebServer::_onRead(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读传来的数据，保存到了client对象中
    if(ret <= 0 && readErrno != EAGAIN) {
        _closeConn(client);
        return;
    }
    onProcess(client);   // 处理数据
}

/**
 * 根据HTTP请求处理结果修改关注的事件类型：
 * 如果有请求报文，则监控写事件；否则继续监控读事件
 * 对于读，只要socket缓冲区中还有数据就一直读：每次读入操作后，主动epoll_mod IN事件，此时只要该fd的缓冲还有数据可以读，则epoll_wait会返回读就绪。
 * 对于写，只要socket缓冲区中还有空间且用户请求写的数据还未写完，就一直写： 每次输出操作后，主动epoll_mod OUT事件，此时只要该fd的缓冲可以发送数据（发送buffer不满），则epoll_wait就会返回写就绪（有时候采用该机制通知epoll_wait醒过来）。
 */
void WebServer::onProcess(HttpConn* client) {
    if(client->process()) {     // 处理 HTTP 请求，解析请求报文并生成响应。客户端请求报文有效 -》 修改为：监听该客户端可写事件
        _epoller->modFd(client->getFd(), _connEvent | EPOLLOUT);
    }
    else {  // 请求报文无效、请求报文不完整 -》 继续监听该客户端可读事件
        _epoller->modFd(client->getFd(), _connEvent | EPOLLIN);     // 注意这行代码的作用，不仅仅是继续监听可读的字面意思，而是再次注册IN事件，相当于一次更新，即实现 socket缓冲区中有数据 还能够立即就绪
    }
}

/**
 * 写事件任务回调：
 * 尝试将响应数据写入客户端，若写完则判断是否保持连接；否则根据返回错误决定是否继续写或关闭连接
 */
void WebServer::_onWrite(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);   // 来到这时，client已经完成了请求的解析，响应的生成。将生成的响应写入对端
    if(client->toWriteBytes() == 0) {   // 数据写完
        if(client->isKeepAlive()) {     // 若需要保持连接
            onProcess(client);          // 根据需要修改监听事件，按理说应该就是直接监听可读事件就行，不知道为啥还要onProcess()。TODO:
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            _epoller->modFd(client->getFd(), _connEvent | EPOLLOUT);
            return;
        }
    }
    // 写失败或数据未全部发送
    _closeConn(client);
}

/**
 * 初始化监听socket：
 * 1. 检查端口有效性；
 * 2. 创建socket并设置SO_LINGER（优雅关闭）与SO_REUSEADDR（地址复用）选项；
 * 3. 绑定地址、监听；
 * 4. 将监听socket加入epoll监控，并设置为非阻塞模式
 */
bool WebServer::_initSocket() {
    int ret;
    struct sockaddr_in addr;
    if(_port > 65535 || _port < 0) {
        LOG_ERROR("Port:%d error!", _port);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_port);
    struct linger optLinger = { 0 };
    if(_openLinger) {
        // 优雅关闭：等待剩余数据发送完毕或超时
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    _listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if(_listenFd < 0) {
        LOG_ERROR("Create socket error!", _port);
        return false;
    }

    ret = setsockopt(_listenFd, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(_listenFd);
        LOG_ERROR("Init linger error!", _port);
        return false;
    }

    ret = bind(_listenFd, (struct sockaddr*) &addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port: %d error!", _port);
        close(_listenFd);
        return false;
    }

    int optval = 1;
    // 端口复用：timewait
    ret = setsockopt(_listenFd, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error!");
        close(_listenFd);
        return false;
    }

    ret = listen(_listenFd, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port: %d error!", _port);
        close(_listenFd);
        return false;
    }

    ret = _epoller->addFd(_listenFd, _listenEvent | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(_listenFd);
        return false;
    }
    setFdNonblock(_listenFd);
    LOG_INFO("Server Port:%d", _port);
    return true;
}

/**
 * 将指定的文件描述符设置为非阻塞模式
 */
int WebServer::setFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

void WebServer::pingSqlCallback() {
    SqlConnPool::instance()->pingIdleConnections();
    _timer->add(33060, 1000*60*60*2, std::bind(&WebServer::pingSqlCallback, this));
}
