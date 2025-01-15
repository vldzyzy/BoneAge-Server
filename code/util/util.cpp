#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <csignal>
#include "util.h"

constexpr size_t MAX_BUFF = 1024;

// 辅助函数：执行read操作，处理错误和中断
ssize_t do_read(int fd, void* buff, size_t n) {
    ssize_t nread;
    while(true) {
        nread = read(fd, buff, n);
        if(nread < 0) {
            if(errno == EINTR) continue;    // 被信号中断，重试
            if(errno == EAGAIN) return 0;   // 非阻塞模式下无数据可读
            return -1;  // 错误
        }
        return nread;   // 成功读取
    }
}

// 读取n个字节到缓冲区
ssize_t readn(int fd, void* buff, size_t n) {
    size_t nleft = n;
    ssize_t readSum = 0;
    char* ptr = static_cast<char*>(buff);

    while(nleft > 0) {
        ssize_t nread = do_read(fd, ptr, nleft);
        if(nread < 0) return -1;
        if(nread == 0) break;   // EOF

        readSum += nread;
        nleft -= nread;
        ptr += nread;
    }
    return readSum;
}

// 读取数据到 string，带zero标志
ssize_t readn(int fd, std::string& sbuff, bool& zero) {
    ssize_t readSum = 0;
    char buff[MAX_BUFF];

    while(true) {
        ssize_t nread = do_read(fd, buff, sizeof(buff));
        if(nread < 0) return -1;
        if(nread == 0) {
            zero = true;
            break;
        }

        readSum += nread;
        sbuff.append(buff, nread);
    }
    return readSum;
}

// 读取数据到string
ssize_t readn(int fd, std::string& sbuff) {
    bool zero = false;
    return readn(fd, sbuff, zero);
}

// 辅助函数：执行write操作，处理错误和中断
ssize_t do_write(int fd, const void* buff, size_t n) {
    ssize_t nwritten;
    while(true) {
        nwritten = write(fd, buff, n);
        if(nwritten < 0) {
            if(errno == EINTR) continue;    // 被信号中断，重试
            if(errno == EAGAIN) return 0;   // 非阻塞模式下无法立即写入
            return -1;  // 错误
        }
        return nwritten;  
    }
}

// 写入n个字节到文件描述符
ssize_t writen(int fd, const void* buff, size_t n) {
    size_t nleft = n;
    ssize_t writeSum = 0;
    const char* ptr = static_cast<const char*>(buff);

    while(nleft > 0) {
        ssize_t nwritten = do_write(fd, ptr, nleft);
        if(nwritten < 0) return -1;
        if(nwritten == 0) break;

        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }
    return writeSum;
}

// 写入string内容到文件描述符
ssize_t writen(int fd, std::string& sbuff) {
    size_t nleft = sbuff.size();
    ssize_t writeSum = 0;
    const char* ptr = sbuff.c_str();

    while(nleft > 0) {
        ssize_t nwritten = do_write(fd, ptr, nleft);
        if(nwritten < 0) return -1;
        if(nwritten == 0) break;

        writeSum += nwritten;
        nleft -= nwritten;
        ptr += nwritten;
    }

    // 更新sbuff，移除已经写入的部分
    if(writeSum > 0) {
        sbuff.erase(0, writeSum);   // 直接移除已经写入的部分
    }
}

// 忽略sigpipe信号
void handle_for_sigpipe() {
    struct sigaction sa = {};
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if(sigaction(SIGPIPE, &sa, nullptr) == -1) {
        perror("sigaction failed");
    }
}

// 设置文件描述符为非阻塞模式
int setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if(flags == -1) return -1;
    if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) return -1;
    return 0;
}

// 设置 TCP_NODELAY选项（禁用Nagle算法）
void setNodelay(int fd) {
    int enable = 1;
    if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) == -1) {
        perror("setsockopt TCP_NODELAY failed");
    }
}

// 设置SO_LINGER选项（延迟关闭）
void setNoLinger(int fd) {
    struct linger linger_opt = {1, 30};     // l_onoff = 1, l_linger = 30
    if(setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt)) == -1) {
        perror("setsockopt SO_LINGER failed");
    }
}

// 关闭写端(SHUT_WR)
void shutDownWR(int fd) {
    if(shutdown(fd, SHUT_WR) == -1) {
        perror("shutdown SHUT_WR failed");
    }
}

// 设置地址复用
int setReuseAddr(int fd) {
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
        perror("Set SO_REUSEADDR failed");
        return -1;
    }
    return 0;
}

// 设置端口复用
int setReusePort(int fd) {
    int on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &on, sizeof(on)) < 0) {
        perror("Set SO_REUSEPORT failed");
        return -1;
    }
    return 0;
}

// 绑定地址和端口
int bindSocket(int fd, const InetAddress& addr) {
    if (bind(fd, 
            reinterpret_cast<const struct sockaddr*>(addr.getInetAddressPtr()), 
            sizeof(struct sockaddr)) < 0) {
        perror("Bind failed");
        return -1;
    }
    return 0;
}

// 开始监听
int listenSocket(int fd, int backlog = SOMAXCONN) {
    if (listen(fd, backlog) < 0) {
        perror("Listen failed");
        return -1;
    }
    return 0;
}

