#include "acceptor.h"
#include "eventloop.h"
#include "inetaddress.h"
#include <fcntl.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

namespace net {

static int OpenIdleFd() {
    return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
}

Acceptor::Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port)
    : loop_(loop)
    , accept_socket_(Socket::CreateNonblockingTCP())
    , idle_fd_(OpenIdleFd()) {

    if (!accept_socket_.IsValid()) {
        throw std::runtime_error("Acceptor CreateNonblockingTCP failed");
    }

    accept_socket_.SetReuseAddr(true);
    if (reuse_port) {
        accept_socket_.SetReusePort(true);
    }

    if (!accept_socket_.Bind(listen_addr)) {
        throw std::runtime_error("Acceptor bind failed");
    }

    accept_channel_ = std::make_unique<Channel>(loop, accept_socket_.GetFd());
    accept_channel_->SetReadCallback([this]() { HandleRead_(); });
}

Acceptor::~Acceptor() {
    accept_channel_->DisableAll();
    if (idle_fd_ >= 0) { ::close(idle_fd_); idle_fd_ = -1; }
}

void Acceptor::Listen() {
    if (listening_) return;
    listening_ = true;
    accept_socket_.Listen();
    accept_channel_->EnableReading();
}

void Acceptor::HandleRead_() {
    loop_->AssertInLoopThread();
    for (;;) {
        InetAddress peer_addr;
        Socket conn_sock = accept_socket_.Accept(&peer_addr);

        if (conn_sock.IsValid()) {
            if (new_connection_callback_) new_connection_callback_(conn_sock.Release(), peer_addr);
            continue;
        }

        int e = errno;
        if (e == EAGAIN || e == EWOULDBLOCK) break; // 无数据可读, 结束
        if (e == EINTR || e == ECONNABORTED) continue; // 被信号中断/客户端断开, 重试

        if (e == EMFILE || e == ENFILE) { // 文件描述符数量达到上限
            ::close(idle_fd_);
            idle_fd_ = ::accept(accept_socket_.GetFd(), nullptr, nullptr);
            ::close(idle_fd_);
            idle_fd_ = OpenIdleFd();
            break;
        }
    }
}

}