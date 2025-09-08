#include "net/tcpconnection.h"
#include "net/eventloop.h"
#include "net/inetaddress.h"
#include "net/socket.h"
#include "net/buffer.h"
#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <unistd.h>
#include <sys/socket.h>

using namespace net;

class TcpConnectionTest : public ::testing::Test {};

// 这是一个全面的端到端测试，验证 TcpConnection 的完整生命周期
TEST_F(TcpConnectionTest, FullLifecycle) {
    EventLoop* loop_ptr = nullptr;
    std::promise<EventLoop*> loop_promise;
    auto loop_future = loop_promise.get_future();

    std::promise<TcpConnection::Ptr> conn_promise;
    auto conn_future = conn_promise.get_future();
    
    std::promise<void> close_promise;
    auto close_future = close_promise.get_future();

    std::atomic<int> connection_cb_count = 0;

    // 1. 启动服务器 EventLoop 线程
    std::thread server_thread([&]() {
        EventLoop loop;
        loop_ptr = &loop;
        loop_promise.set_value(loop_ptr);
        loop.Loop();
    });
    EventLoop* loop = loop_future.get();

    // 2. 在主线程创建 listen socket
    Socket listen_sock = Socket::CreateNonblockingTCP();
    InetAddress listen_addr("127.0.0.1", 0);
    listen_sock.SetReuseAddr(true);
    ASSERT_TRUE(listen_sock.Bind(listen_addr));
    ASSERT_TRUE(listen_sock.Listen());

    struct sockaddr_in actual_addr;
    socklen_t len = sizeof(actual_addr);
    ASSERT_EQ(::getsockname(listen_sock.GetFd(), (struct sockaddr*)&actual_addr, &len), 0);
    InetAddress server_addr(actual_addr);

    // 3. 在主线程创建 client socket 并连接
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);
    ASSERT_EQ(::connect(client_fd, server_addr.GetSockAddr(), sizeof(struct sockaddr_in)), 0);

    // 4. 在主线程 accept 连接
    InetAddress peer_addr;
    Socket conn_socket = listen_sock.Accept(&peer_addr);
    ASSERT_TRUE(conn_socket.IsValid());
    int conn_fd = conn_socket.Release();

    // 5. 在 EventLoop 线程中创建 TcpConnection
    loop->RunInLoop([&]() {
        TcpConnection::Ptr conn = std::make_shared<TcpConnection>(loop, conn_fd, "a", server_addr, peer_addr);
        
        conn->SetConnectionCallback([&](const TcpConnection::Ptr& c) {
            connection_cb_count++;
            if (c->IsConnected()) {
                // 连接建立时的断言
                SUCCEED();
            } else {
                // 连接断开时的断言
                SUCCEED();
            }
        });

        conn->SetMessageCallback([&](const TcpConnection::Ptr& c, Buffer& buf) {
            std::string msg = buf.RetrieveAllToString();
            EXPECT_EQ(msg, "hello");
            c->Send("world");
        });

        conn->SetCloseCallback([&](const TcpConnection::Ptr& c) {
            close_promise.set_value();
        });

        conn->ConnectEstablished();
        conn_promise.set_value(conn);
    });

    TcpConnection::Ptr conn = conn_future.get();

    // 6. 测试数据收发
    const char* req = "hello";
    ssize_t n = ::write(client_fd, req, strlen(req));
    ASSERT_EQ(n, strlen(req));

    char resp_buf[10];
    n = ::read(client_fd, resp_buf, sizeof(resp_buf));
    ASSERT_EQ(n, 5);
    ASSERT_EQ(std::string(resp_buf, n), "world");

    // 7. 测试优雅关闭 (Server-side shutdown)
    conn->Shutdown();
    n = ::read(client_fd, resp_buf, sizeof(resp_buf));
    ASSERT_EQ(n, 0);

    // 8. 测试连接销毁 (Client-side close)
    ::close(client_fd);

    ASSERT_EQ(close_future.wait_for(std::chrono::seconds(1)), std::future_status::ready);
    ASSERT_EQ(connection_cb_count.load(), 2);

    // 9. 清理
    loop->Quit();
    server_thread.join();
}