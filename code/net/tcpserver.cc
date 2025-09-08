#include "tcpserver.h"
#include "acceptor.h"
#include "eventloop.h"
#include "eventloopthreadpool.h"
#include <cassert>
#include <memory>
#include <string>

namespace net {

TcpServer::TcpServer(const InetAddress& listen_addr, const std::string& name)
    : loop_(std::make_unique<EventLoop>()),
      name_(name),
      acceptor_(std::make_unique<Acceptor>(loop_.get(), listen_addr)),
      thread_pool_(std::make_unique<EventLoopThreadPool>(0)) {
    
    acceptor_->SetNewConnectionCallback(
        [this](int sockfd, const InetAddress& peer_addr) {
            this->NewConnection_(sockfd, peer_addr);
        }
    );
}

TcpServer::~TcpServer() {
    loop_->AssertInLoopThread();
    for (auto& item : connections_) {
        TcpConnection::Ptr conn(item.second);
        item.second.reset();
        conn->GetLoop()->RunInLoop([conn]() {
            conn->ConnectDestroyed();
        });
    }
}

void TcpServer::SetThreadNum(int num_threads) {
    assert(num_threads >= 0);
    thread_pool_ = std::make_unique<EventLoopThreadPool>(num_threads);
}

void TcpServer::Start() {
    if (!started_) {
        started_ = true;
        thread_pool_->Start();
        loop_->RunInLoop([this]() {
            acceptor_->Listen();
        });
    }
    loop_->Loop();
}

// 在 main Reactor 中执行
void TcpServer::NewConnection_(int sockfd, const InetAddress& peer_addr) {
    loop_->AssertInLoopThread();
    
    // 从线程池中挑选一个 sub-reactor
    EventLoop* io_loop = thread_pool_->GetNextLoop();
    if (io_loop == nullptr) { // 如果没有设置线程池，就使用 main reactor
        io_loop = loop_.get();
    }

    std::string conn_name = name_ + "#" + std::to_string(next_conn_id_++);
    
    // 获取本地地址
    struct sockaddr_in local;
    socklen_t addrlen = sizeof(local);
    ::getsockname(sockfd, (struct sockaddr*)&local, &addrlen);
    InetAddress local_addr(local);

    TcpConnection::Ptr conn = std::make_shared<TcpConnection>(io_loop, sockfd, conn_name, local_addr, peer_addr);
    connections_[conn_name] = conn;

    conn->SetConnectionCallback(connection_callback_);
    conn->SetMessageCallback(message_callback_);
    conn->SetCloseCallback(
        [this](const TcpConnection::Ptr& c) {
            this->RemoveConnection_(c);
        }
    );
    
    // 在 sub-reactor 线程中执行连接建立
    io_loop->RunInLoop([conn]() {
        conn->ConnectEstablished();
    });
}

void TcpServer::RemoveConnection_(const TcpConnection::Ptr& conn) {
    loop_->RunInLoop([this, conn]() {
        this->RemoveConnectionInLoop_(conn);
    });
}

void TcpServer::RemoveConnectionInLoop_(const TcpConnection::Ptr& conn) {
    loop_->AssertInLoopThread();
    
    size_t n = connections_.erase(conn->GetName());
    assert(n == 1);

    EventLoop* io_loop = conn->GetLoop();
    io_loop->QueueInLoop([conn]() {
        conn->ConnectDestroyed();
    });
}

} // namespace net
