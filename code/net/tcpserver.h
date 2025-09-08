#pragma once

#include "tcpconnection.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace net {

class EventLoop;
class Acceptor;
class EventLoopThreadPool;

class TcpServer {
public:
    TcpServer(const InetAddress& listen_addr, const std::string& name);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // 设置 sub-reactor (IO 线程) 的数量
    void SetThreadNum(int num_threads);

    void Start();

    void SetConnectionCallback(const TcpConnection::ConnectionCallback& cb) { connection_callback_ = cb; }
    void SetMessageCallback(const TcpConnection::MessageCallback& cb) { message_callback_ = cb; }

private:
    void NewConnection_(int sockfd, const InetAddress& peer_addr);
    void RemoveConnection_(const TcpConnection::Ptr& conn);
    void RemoveConnectionInLoop_(const TcpConnection::Ptr& conn);

    using ConnectionMap = std::unordered_map<std::string, TcpConnection::Ptr>;

    std::unique_ptr<EventLoop> loop_;
    const std::string name_;
    
    std::unique_ptr<Acceptor> acceptor_;
    std::unique_ptr<EventLoopThreadPool> thread_pool_;

    TcpConnection::ConnectionCallback connection_callback_;
    TcpConnection::MessageCallback message_callback_;

    bool started_{false};
    int next_conn_id_{1};
    ConnectionMap connections_;
};

} // namespace net
