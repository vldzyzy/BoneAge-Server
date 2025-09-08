#include "net/socket.h"
#include "net/inetaddress.h"
#include <gtest/gtest.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>

using namespace net;

class SocketTest : public ::testing::Test {};

// 测试1：RAII - 验证 Socket 析构时 fd 被正确关闭
TEST_F(SocketTest, RAII) {
    int fd = -1;
    {
        Socket sock = Socket::CreateNonblockingTCP();
        ASSERT_TRUE(sock.IsValid());
        fd = sock.GetFd();
    } // sock 在这里被析构，应该调用 close(fd)

    // 验证 fd 是否真的被关闭了
    // 任何对已关闭 fd 的操作都会失败，通常返回-1，errno 设置为 EBADF
    int ret = ::fcntl(fd, F_GETFL);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(errno, EBADF); // Bad file descriptor
}

// 测试2：移动语义
TEST_F(SocketTest, MoveSemantics) {
    Socket sock1 = Socket::CreateNonblockingTCP();
    const int fd1 = sock1.GetFd();
    ASSERT_TRUE(sock1.IsValid());

    // 测试移动构造
    Socket sock2(std::move(sock1));
    EXPECT_FALSE(sock1.IsValid()); // sock1 应该失效
    EXPECT_EQ(sock2.GetFd(), fd1); // sock2 获得 fd

    Socket sock3 = Socket::CreateNonblockingTCP();
    const int fd3 = sock3.GetFd();

    // 测试移动赋值
    sock2 = std::move(sock3);
    EXPECT_FALSE(sock3.IsValid()); // sock3 应该失效
    EXPECT_EQ(sock2.GetFd(), fd3); // sock2 获得新的 fd3
    
    // 验证 sock2 原来的 fd1 是否被关闭
    int ret = ::fcntl(fd1, F_GETFL);
    ASSERT_EQ(ret, -1);
    ASSERT_EQ(errno, EBADF);
}

// 测试3：Socket 选项
TEST_F(SocketTest, SocketOptions) {
    Socket sock = Socket::CreateNonblockingTCP();
    ASSERT_TRUE(sock.IsValid());

    // 测试 SetReuseAddr
    sock.SetReuseAddr(true);
    int optval = 0;
    socklen_t optlen = sizeof(optval);
    int ret = ::getsockopt(sock.GetFd(), SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(optval, 0); // true 应该是非0值

    // 测试 SetKeepAlive
    sock.SetKeepAlive(true);
    optval = 0;
    ret = ::getsockopt(sock.GetFd(), SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen);
    ASSERT_EQ(ret, 0);
    EXPECT_NE(optval, 0);
}

// 测试4：Bind, Listen, Accept 完整流程
TEST_F(SocketTest, BindListenAccept) {
    // 1. 创建监听 Socket
    Socket listen_sock = Socket::CreateNonblockingTCP();
    listen_sock.SetReuseAddr(true);

    // 2. 绑定到一个临时地址
    InetAddress listen_addr("127.0.0.1", 0); // 端口0让系统自动选择
    ASSERT_TRUE(listen_sock.Bind(listen_addr));

    // 获取系统分配的端口号
    struct sockaddr_in actual_addr;
    socklen_t len = sizeof(actual_addr);
    ASSERT_EQ(::getsockname(listen_sock.GetFd(), (struct sockaddr*)&actual_addr, &len), 0);
    InetAddress server_addr(actual_addr);

    // 3. 开始监听
    ASSERT_TRUE(listen_sock.Listen());

    // 4. 创建客户端 Socket 并连接
    int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(client_fd, 0);
    int ret = ::connect(client_fd, server_addr.GetSockAddr(), sizeof(struct sockaddr_in));
    ASSERT_EQ(ret, 0);

    // 5. 接受连接
    InetAddress peer_addr;
    Socket conn_sock = listen_sock.Accept(&peer_addr);
    ASSERT_TRUE(conn_sock.IsValid());

    // 6. 验证对端地址是否正确 (IP 应该匹配)
    EXPECT_EQ(peer_addr.ToIp(), "127.0.0.1");

    // 清理
    ::close(client_fd);
}