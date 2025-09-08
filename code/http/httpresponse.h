#pragma once
#include <string>
#include <unordered_map>
#include "net/buffer.h"

namespace http {

class HttpResponse {
public:
    HttpResponse() { Reset(); }
    
    void Reset();

    void SetStatusCode(int code);

    void SetStatusMessage(std::string message);

    void SetKeepAlive(bool on);

    void SetContentType(std::string content_type);

    void SetHeader(std::string key, std::string value);

    void SetBody(std::string body);

    void AppendToBuffer(net::Buffer& buffer);

    bool IsKeepAlive() const { return is_keep_alive_; }

private:
    int status_code_;
    std::string status_message_;
    bool is_keep_alive_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
};

}