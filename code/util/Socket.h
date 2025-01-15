#pragma once
#include <unistd.h>
#include <stdexcept>
#include <utility>
#include <string>
#include <string_view>
#include <vector>
#include <system_error>
#include <errno.h>
#include <sys/socket.h>

/**
 * @brief Socket文件描述符封装类
 * 采用RAII方式管理socket文件描述符的生命周期
 */
class Socket {
public:
    // 创建新的socket
    Socket() : _fd(::socket(AF_INET, SOCK_STREAM, 0)) {
        if (_fd < 0) {
            throw std::runtime_error("Socket creation failed");
        }
    }

    // 通过已有文件描述符构造
    explicit Socket(int fd) noexcept : _fd(fd) {}

    // 析构时自动关闭socket
    ~Socket() noexcept {
        if (_fd >= 0) {
            ::close(_fd);
        }
    }

    // 禁用拷贝
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // 移动语义实现
    Socket(Socket&& other) noexcept : _fd(std::exchange(other._fd, -1)) {}
    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            if (_fd >= 0) {
                ::close(_fd);
            }
            _fd = std::exchange(other._fd, -1);
        }
        return *this;
    }

    [[nodiscard]] int getFd() const noexcept { return _fd; }

    /**
     * @brief 释放文件描述符的所有权
     * @return 文件描述符，Socket 类不再管理它
     */
    [[nodiscard]] int releaseFd() noexcept {
        int fd = _fd;
        _fd = -1; // 将文件描述符标记为无效，Socket 不再管理该资源
        return fd;
    }

private:
    int _fd; // socket文件描述符
};

/**
 * @brief Socket读写操作封装类
 * 提供可靠的数据收发功能
 */
class SocketIO {
public:
    explicit SocketIO(int fd) noexcept : _fd(fd) {}

    // 禁止拷贝
    SocketIO(const SocketIO&) = delete;
    SocketIO& operator=(const Socket&) = delete;

    // 读取指定字节数的数据到vector中
    [[nodiscard]] ssize_t readn(std::vector<char>& buffer, size_t n) {
        buffer.resize(n);
        return readn(buffer.data(), n);
    }

    // 读取指定字节数的数据，处理中断和部分读取的情况
    [[nodiscard]] ssize_t readn(char* buf, size_t len) {
        size_t nleft = len;
        ssize_t nread;
        char* ptr = buf;

        while (nleft > 0) {
            if ((nread = recv(_fd, ptr, nleft, 0)) < 0) {
                if (errno == EINTR) { // 处理系统调用中断
                    nread = 0;
                }
                else {
                    throw std::system_error(errno, std::system_category(), "Recv failed");
                }
            }
            else if (nread == 0) { // 对端关闭连接
                break;
            }
            nleft -= nread;
            ptr += nread;
        }
        return len - nleft;
    }

    // 写入字符串数据
    void writen(std::string_view data) {
        writen(data.data(), data.size());
    }

    // 写入指定字节数的数据，处理中断和部分写入的情况
    void writen(const char* buf, size_t len) {
        size_t nleft = len;
        ssize_t nwritten;
        const char* ptr = buf;

        while (nleft > 0) {
            if ((nwritten = send(_fd, ptr, nleft, MSG_NOSIGNAL)) <= 0) {
                if (nwritten < 0 && errno == EINTR) {
                    nwritten = 0;
                }
                else {
                    throw std::system_error(errno, std::system_category(), "Send failed");
                }
            }
            nleft -= nwritten;
            ptr += nwritten;
        }
    }

    [[nodiscard]] std::string readline(size_t maxlen = 4096) {
        std::string line;

        while (line.size() < maxlen - 1) {
            // 检查缓存中是否有完整的一行
            auto pos = _buffer.find('\n');
            if (pos != std::string::npos) {
                // 找到一行，提取到 `line` 中
                line = _buffer.substr(0, pos + 1);  // 包含 '\n'
                _buffer.erase(0, pos + 1);         // 移除已经读取的数据
                return line;
            }

            // 如果没有完整的一行，从内核缓冲区继续读取
            std::vector<char> temp_buffer(1024);  // 临时缓冲区
            ssize_t bytes_read = recv(_fd, temp_buffer.data(), temp_buffer.size(), 0);

            if (bytes_read > 0) {
                // 将读取的数据追加到 `_buffer`
                _buffer.append(temp_buffer.data(), bytes_read);
            } else if (bytes_read == 0) {
                // 对端关闭连接，返回已有数据（如果有）
                if (!_buffer.empty()) {
                    line = _buffer;
                    _buffer.clear();
                    return line;
                }
                break;
            } else if (errno == EINTR) {
                continue;  // 中断重试
            } else {
                throw std::system_error(errno, std::system_category(), "Read failed");
            }
        }

        return line;
    }

private:
    const int _fd; // socket文件描述符
    std::string _buffer;  // 缓存区，用于存储未处理的数据
};