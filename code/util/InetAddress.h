#pragma once

#include <string>
#include <netinet/in.h>

/**
 * @brief 网络地址封装类
 * 封装IP地址和端口号，提供地址转换功能
 */
class InetAddress {
public:
    InetAddress() = default;
    // 从IP和端口构造
    InetAddress(const std::string& ip, unsigned short port);

    // 直接从网络地址结构体构造
    explicit InetAddress(const struct sockaddr_in& addr) noexcept;

    // 禁用拷贝
    InetAddress(const InetAddress&) = delete;
    InetAddress& operator=(const InetAddress&) = delete;

    // 启用移动
    InetAddress(InetAddress&&) noexcept;
    InetAddress& operator=(InetAddress&&) noexcept;

    ~InetAddress() noexcept;

    // 获取IP地址的字符串表示
    [[nodiscard]] std::string ip() const;

    // 获取端口号
    [[nodiscard]] uint16_t port() const noexcept;

    // 获取底层sockaddr_in结构体指针
    [[nodiscard]] const struct sockaddr_in* getInetAddressPtr() const noexcept;

private:
    struct sockaddr_in _addr;
};
