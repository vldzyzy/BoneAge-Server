#include "inetaddress.h"
#include <cstring> // For memset
#include <stdexcept>
#include <string>

namespace net {

InetAddress::InetAddress(const std::string& ip, uint16_t port) {
    memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
        throw std::runtime_error("inet_pton failed");
    }
}

std::string InetAddress::ToIp() const {
    char buf[64] = "";
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::ToIpPort() const {
    char buf[64] = "";
    inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ToPort();
    snprintf(buf + end, sizeof(buf) - end, ":%u", port);
    return buf;
}

uint16_t InetAddress::ToPort() const {
    return ntohs(addr_.sin_port);
}

}