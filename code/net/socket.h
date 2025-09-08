#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace net {

class InetAddress;

class Socket {
public:
    static Socket CreateNonblockingTCP();

    explicit Socket(int sockfd = -1) noexcept : sockfd_(sockfd) {}
    ~Socket();

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    int GetFd() const noexcept { return sockfd_; }
    bool IsValid() const noexcept { return sockfd_ >= 0; }

    int Release() noexcept;

    bool Bind(const InetAddress& addr) noexcept;
    bool Listen(int backlog = SOMAXCONN) noexcept;

    Socket Accept(InetAddress* peer_addr) noexcept;

    void ShutdownWrite() noexcept;

    void SetTcpNoDelay(bool on) noexcept;
    void SetReuseAddr(bool on) noexcept;
    void SetReusePort(bool on) noexcept; 
    void SetKeepAlive(bool on) noexcept;

private:
    int sockfd_;
};

} // namespace net
