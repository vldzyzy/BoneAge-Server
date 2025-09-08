#include "socket.h"
#include "inetaddress.h"
#include <fcntl.h>
#include <cerrno>

namespace net {

Socket Socket::CreateNonblockingTCP() {
    int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (fd < 0) return Socket(-1);
    return Socket(fd);
}

Socket::~Socket() {
    if (sockfd_ >= 0) {
        ::close(sockfd_);
        sockfd_ = -1;
    }
}

Socket::Socket(Socket&& other) noexcept : sockfd_(other.sockfd_) {
    other.sockfd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (sockfd_ >= 0) ::close(sockfd_);
        sockfd_ = other.sockfd_;
        other.sockfd_ = -1;
    }
    return *this;
}

int Socket::Release() noexcept {
    int fd = sockfd_;
    sockfd_ = -1;
    return fd;
}

bool Socket::Bind(const InetAddress& addr) noexcept {
    if (sockfd_ < 0) { errno = EBADF; return false; }
    return ::bind(sockfd_, addr.GetSockAddr(), sizeof(sockaddr_in)) == 0;
}

bool Socket::Listen(int backlog) noexcept {
    if (sockfd_ < 0) { errno = EBADF; return false; }
    return ::listen(sockfd_, backlog) == 0;
}

Socket Socket::Accept(InetAddress* peer_addr) noexcept {
    if (sockfd_ < 0) { errno = EBADF; return Socket(-1); }

    sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    int connfd = ::accept4(sockfd_, reinterpret_cast<sockaddr*>(&addr),
                           &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
    if (connfd < 0) {
        return Socket(-1);
    }
    if (peer_addr) peer_addr->SetSockAddr(addr);
    return Socket(connfd);
}

void Socket::ShutdownWrite() noexcept {
    if (sockfd_ >= 0) ::shutdown(sockfd_, SHUT_WR);
}

void Socket::SetTcpNoDelay(bool on) noexcept {
    if (sockfd_ < 0) return;
    int opt = on ? 1 : 0;
    ::setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
}

void Socket::SetReuseAddr(bool on) noexcept {
    if (sockfd_ < 0) return;
    int opt = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}

void Socket::SetReusePort(bool on) noexcept {
    if (sockfd_ < 0) return;
    int opt = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
}

void Socket::SetKeepAlive(bool on) noexcept {
    if (sockfd_ < 0) return;
    int opt = on ? 1 : 0;
    ::setsockopt(sockfd_, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
}

} // namespace net
