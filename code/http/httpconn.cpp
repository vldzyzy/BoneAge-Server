#include "httpconn.h"


// 初始化静态变量
const char* HttpConn::srcDir = nullptr;
std::atomic<int> HttpConn::userCount(0);
bool HttpConn::isET = false;

HttpConn::HttpConn()
    : _sockfd(-1)
    , _addr({ 0 })
    , _isClose(true) {}

HttpConn::~HttpConn() {
    close();
}

// 初始化一个新的HTTP连接
void HttpConn::init(int sockFd, const sockaddr_in& addr) {
    assert(sockFd > 0);
    userCount++;    // 连接数加1
    _addr = addr;
    _sockfd = sockFd;
    // 清空读写缓冲区，准备新的连接使用
    _writeBuff.retrieveAll();
    _readBuff.retrieveAll();
    _isClose = false;
    LOG_INFO("Client[%d](%s:%d) in, userCount:%d", _sockfd, getIP(), getPort(), (int)userCount);
}

// 关闭当前连接，释放资源
void HttpConn::close() {
    // 解除文件映射
    _response.unmapFile();
    if(!_isClose) {
        _isClose = true;
        userCount--;
        ::close(_sockfd);
        LOG_INFO("Client[%d](%s:%d) quit, userCount:%d", _sockfd, getIP(), getPort(), (int)userCount);
    }
}

// 获取socket文件描述符
int HttpConn::getFd() const {
    return _sockfd;
}

// 获取客户端地址结构体
struct sockaddr_in HttpConn::getAddr() const {
    return _addr;
}

// 获取客户端 IP 地址字符串
const char* HttpConn::getIP() const {
    return inet_ntoa(_addr.sin_addr);
}

// 获取客户端端口号
int HttpConn::getPort() const {
    return _addr.sin_port;
}

/**
 * @brief 从 socket 中读取数据，存入读缓冲区
 * 
 * @param saveErrno 指针，用于保存错误码
 * @return ssize_t 返回读取的字节数，若发生错误返回 -1
 */
ssize_t HttpConn::read(int* saveErrno) {
    ssize_t len = -1;
    do {
        // 从 _sockfd 读取数据到 _readBuff 中
        len = _readBuff.readFd(_sockfd, saveErrno);
        if (len <= 0) {
            // 没有数据或发生错误则退出循环
            break;
        }
    } while (isET);   // 若为边沿触发模式则循环读取
    return len;
}

/**
 * @brief 将写缓冲区的数据通过 writev 写入 socket 中
 * 
 * @param saveErrno 指针，用于保存错误码
 * @return ssize_t 返回写入的字节数，若发生错误返回 -1
 */
ssize_t HttpConn::write(int* saveErrno) {
    ssize_t len = -1;
    do {
        // 使用 writev 将 _iov 中的数据写入 socket
        len = writev(_sockfd, _iov, _iovCnt);
        if (len <= 0) {
            *saveErrno = errno;
            break;
        }
        // 判断是否所有数据都已经发送完毕
        if (_iov[0].iov_len + _iov[1].iov_len == 0) {
            // 数据发送完毕，退出循环
            break;
        }
        // 处理部分写入的情况
        else if (static_cast<size_t>(len) > _iov[0].iov_len) {
            // 表示响应头全部发送完毕，部分响应体被发送
            // 更新 _iov[1] 指针和长度，去掉已经发送的数据
            _iov[1].iov_base = (uint8_t*)_iov[1].iov_base + (len - _iov[0].iov_len);
            _iov[1].iov_len -= (len - _iov[0].iov_len);
            if (_iov[0].iov_len) {
                // 清空写缓冲区（响应头部分）
                _writeBuff.retrieveAll();
                _iov[0].iov_len = 0;
            }
        }
        else {
            // 若仅部分响应头被写入，则更新 _iov[0]
            _iov[0].iov_base = (uint8_t*)_iov[0].iov_base + len;
            _iov[0].iov_len -= len;
            _writeBuff.retrieve(len);
        }
    } while (isET || toWriteBytes() > 10240);  // 条件可以根据实际需要调整
    return len;
}

/**
 * @brief 处理 HTTP 请求，解析请求报文并生成响应
 * 
 * @return bool 请求是否成功处理（返回 true 表示请求有效）
 */
bool HttpConn::process() {
    // 重置请求对象，准备解析新的请求报文, 但是在处理读取 长http请求时 不应该重置
    // _request.init();  
    // 如果读缓冲区没有可读数据，则返回 false
    if(_readBuff.readableBytes() <= 0) {
        return false;
    }
    // 尝试解析 HTTP 请求报文
    HTTP_CODE ret = _request.parse(_readBuff);
    // 请求不完整，接着读socket缓冲区内容
    if(ret == NO_REQUEST) {
        return false;   
    }
    else if(ret == GET_REQUEST) {
        LOG_DEBUG("%s", _request.path().c_str());
        // 判断是否为算法推理请求：例如 POST 方法且请求路径为 "/predict"
        if(_request.method() == "POST" && _request.path() == "/detect") {
            // 创建推理任务，提交到推理任务队列
            inferenceTask task(_sockfd, _request.getPostPtr());
            std::future<std::string> result_future = task.resultPromise.get_future();
            inferenceQueue.push_back(std::move(task));

            std::string inferenceResult = result_future.get();

            // 使用带模型推理的 init 接口
            _response.init(srcDir, _request.path(), _request.isKeepAlive(), 200, inferenceResult);
        }
        else {  // 静态资源
            // 如果解析成功，根据请求生成响应，状态码设为 200
            _response.init(srcDir, _request.path(), _request.isKeepAlive(), 200); 
        }
        _request.init(); // 如果是长连接，等待下一次请求，需要初始化
    } 
    else {    
        // 解析失败则生成 400 错误响应
        _response.init(srcDir, _request.path(), false, 400);
    }
    // 根据生成的响应，构造完整的响应报文（包括头部和正文）
    _response.makeResponse(_writeBuff);
    // 准备写操作的 iovec 数组
    // 第一块数据：响应头和部分响应内容在 _writeBuff 中
    _iov[0].iov_base = const_cast<char*>(_writeBuff.peek());
    _iov[0].iov_len = _writeBuff.readableBytes();
    _iovCnt = 1;

    // 如果响应包含文件（例如请求静态资源），则将文件映射到内存，并加入 iovec 数组
    if(_response.fileLen() > 0  && _response.file()) {
        _iov[1].iov_base = _response.file();
        _iov[1].iov_len = _response.fileLen();
        _iovCnt = 2;
    }
    LOG_DEBUG("filesize:%d, iovCnt: %d, Bytes to write: %d", _response.fileLen() , _iovCnt, toWriteBytes());
    return true;
}