#include "net/acceptor.h"
#include "net/eventloop.h"
#include "net/inetaddress.h"
#include "net/socket.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <unistd.h>

using namespace net;

class AcceptorTest : public ::testing::Test {};

// 测试 Acceptor 的完整流程：构造、监听、接受连接
TEST_F(AcceptorTest, ReliableAcceptanceFlow) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> loop_promise;
    auto loop_future = loop_promise.get_future();

    std::promise<InetAddress> addr_promise;
    auto addr_future = addr_promise.get_future();
    
    std::promise<int> fd_promise;
    auto fd_future = fd_promise.get_future();

    // 1. 启动服务器 EventLoop 线程
    std::thread t([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        loop_promise.set_value(loop_ptr);
        
        InetAddress listen_addr("127.0.0.1", 0); // 使用端口0，让内核选择
        Acceptor acceptor(&loop, listen_addr, true); // reuse_port
        
        acceptor.SetNewConnectionCallback([&](int sockfd, const InetAddress& peer_addr) {
            // 当新连接到来时，通过 promise 把 fd 送回主线程
            fd_promise.set_value(sockfd);
        });
        
        acceptor.Listen();
        
        // 关键一步：获取实际绑定的地址并送回主线程
        struct sockaddr_in local;
        socklen_t addrlen = sizeof(local);
        // 使用我们新增的 GetFd() 方法
        ::getsockname(acceptor.GetFd(), (struct sockaddr*)&local, &addrlen);
        addr_promise.set_value(InetAddress(local));
        
        loop.Loop();
    });

    // 2. 主线程等待，直到拿到服务器的 EventLoop 指针和确切地址
    EventLoop* loop = loop_future.get();
    InetAddress server_addr = addr_future.get();
    
    // 3. 主线程扮演客户端，发起连接
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);
    ASSERT_EQ(::connect(client_fd, server_addr.GetSockAddr(), sizeof(sockaddr_in)), 0);
    
    // 4. 主线程等待，直到 NewConnectionCallback 被触发并返回新连接的 fd
    // 设置超时以防测试卡死
    auto status = fd_future.wait_for(std::chrono::seconds(1));
    ASSERT_EQ(status, std::future_status::ready);
    int new_conn_fd = fd_future.get();
    ASSERT_GE(new_conn_fd, 0);
    
    // 5. 清理
    ::close(client_fd);
    ::close(new_conn_fd);
    loop->Quit();
    t.join();
}

