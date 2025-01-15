#include "InetAddress.h"
#include <cstring>
#include <arpa/inet.h>
#include <stdexcept>

InetAddress::InetAddress(const std::string& ip, unsigned short port) {
    memset(&_addr, 0, sizeof(_addr));
    _addr.sin_family = AF_INET;
    _addr.sin_port = htons(port);
    // 将IP地址从字符串转换为网络字节序
    if (inet_pton(AF_INET, ip.c_str(), &_addr.sin_addr) <= 0) {
        throw std::runtime_error("Invalid IP address");
    }
}

InetAddress::InetAddress(const struct sockaddr_in& addr) noexcept : _addr(addr) {}

std::string InetAddress::ip() const {
    char buf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &_addr.sin_addr, buf, sizeof(buf))) {
        throw std::runtime_error("Failed to convert IP address");
    }
    return buf;
}

uint16_t InetAddress::port() const noexcept {
    return ntohs(_addr.sin_port);
}

const struct sockaddr_in* InetAddress::getInetAddressPtr() const noexcept {
    return &_addr;
}
