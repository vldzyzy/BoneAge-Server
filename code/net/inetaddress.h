#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <cstring>

namespace net {

class InetAddress {
public:
    InetAddress() { memset(&addr_, 0, sizeof(addr_)); }
    
    InetAddress(const std::string& ip, uint16_t port);

    explicit InetAddress(const struct sockaddr_in& addr) : addr_(addr) {}

    std::string ToIp() const;

    std::string ToIpPort() const;

    uint16_t ToPort() const;

    const struct sockaddr* GetSockAddr() const {
        return reinterpret_cast<const struct sockaddr*>(&addr_);
    }
    struct sockaddr* GetSockAddr() {
        return reinterpret_cast<struct sockaddr*>(&addr_);
    }
    
    void SetSockAddr(const struct sockaddr_in& addr) { addr_ = addr; }

private:
    struct sockaddr_in addr_;
};

}