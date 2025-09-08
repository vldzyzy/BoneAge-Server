#pragma once

#include <functional>
#include "net/eventloop.h"
#include "socket.h"
#include "channel.h"

namespace net {

class EventLoop;
class InetAddress;

class Acceptor {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress&)>;

    Acceptor(EventLoop* loop, const InetAddress& listen_addr, bool reuse_port = false);
    ~Acceptor();

    Acceptor(const Acceptor&) = delete;
    Acceptor& operator=(const Acceptor&) = delete;

    void SetNewConnectionCallback(NewConnectionCallback cb) {
        new_connection_callback_ = std::move(cb);
    }
    // main reactor io线程执行
    void Listen();

    int GetFd() const { return accept_socket_.GetFd(); }

private:
    void HandleRead_();

    EventLoop* loop_;
    Socket accept_socket_;
    std::unique_ptr<Channel> accept_channel_;
    NewConnectionCallback new_connection_callback_;
    bool listening_{false};
    int idle_fd_{-1};
};

}