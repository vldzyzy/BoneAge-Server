#include "net/inetaddress.h" // 确保路径正确
#include <gtest/gtest.h>

using namespace net;

// 对于这种简单的值对象，Test Fixture 不是必须的，但保持结构一致性
class InetAddressTest : public ::testing::Test {};

// 测试1：有效的 IP 和端口构造
TEST_F(InetAddressTest, ValidIpAndPort) {
    InetAddress addr("127.0.0.1", 8080);

    EXPECT_EQ(addr.ToIp(), "127.0.0.1");
    EXPECT_EQ(addr.ToPort(), 8080);
    EXPECT_EQ(addr.ToIpPort(), "127.0.0.1:8080");

    // 深入检查内部结构
    const struct sockaddr_in* raw_addr = reinterpret_cast<const struct sockaddr_in*>(addr.GetSockAddr());
    EXPECT_EQ(raw_addr->sin_family, AF_INET);
    EXPECT_EQ(ntohs(raw_addr->sin_port), 8080);
    // 将网络字节序的地址转换回字符串进行比较
    char ip_buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &raw_addr->sin_addr, ip_buf, sizeof(ip_buf));
    EXPECT_STREQ(ip_buf, "127.0.0.1");
}

// 测试2：监听地址 (INADDR_ANY)
TEST_F(InetAddressTest, AnyAddress) {
    // 通常用于服务器监听
    InetAddress addr("0.0.0.0", 9999);

    EXPECT_EQ(addr.ToIp(), "0.0.0.0");
    EXPECT_EQ(addr.ToPort(), 9999);
    EXPECT_EQ(addr.ToIpPort(), "0.0.0.0:9999");

    const struct sockaddr_in* raw_addr = reinterpret_cast<const struct sockaddr_in*>(addr.GetSockAddr());
    EXPECT_EQ(raw_addr->sin_addr.s_addr, htonl(INADDR_ANY));
}

// 测试3：从 sockaddr_in 构造
TEST_F(InetAddressTest, ConstructFromSockAddr) {
    struct sockaddr_in raw_addr;
    memset(&raw_addr, 0, sizeof(raw_addr));
    raw_addr.sin_family = AF_INET;
    raw_addr.sin_port = htons(1234);
    inet_pton(AF_INET, "192.168.1.1", &raw_addr.sin_addr);

    InetAddress addr(raw_addr);

    EXPECT_EQ(addr.ToIp(), "192.168.1.1");
    EXPECT_EQ(addr.ToPort(), 1234);
    EXPECT_EQ(addr.ToIpPort(), "192.168.1.1:1234");
}

// 测试4：无效的 IP 地址
TEST_F(InetAddressTest, InvalidIpShouldThrow) {
    // inet_pton 对于无效IP会失败，构造函数应该抛出异常
    EXPECT_THROW(InetAddress("256.1.2.3", 80), std::runtime_error);
    EXPECT_THROW(InetAddress("not an ip", 80), std::runtime_error);
    EXPECT_THROW(InetAddress("192.168.1", 80), std::runtime_error);
}