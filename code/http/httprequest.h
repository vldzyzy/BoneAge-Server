#pragma once
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include "net/buffer.h"
#include <optional>

namespace http {

class HttpRequest {
public:
    enum class HttpCode {
        kNoRequest,     // http请求 解析还没完成，继续解析
        kGetRequest,    // 解析完成
        kBadRequest,    // 错误的请求内容
    };

    HttpRequest() { Reset(); }

    ~HttpRequest() = default;

    void Reset();

    HttpCode Parse(net::Buffer& buff);

    const std::string& GetMethod() const { return method_; }
    const std::string& GetPath() const { return path_; }
    const std::string& GetVersion() const { return version_; }
    const std::string& GetBody() const { return body_; }
    const std::unordered_map<std::string, std::string>& GetHeaders() const { return headers_; }
    bool IsKeepAlive() const { return is_keep_alive_; }

private:
    enum class ParseState {
        kRequestLine,
        kHeaders,
        kBody,
        kFinish,
    };

    std::optional<std::string_view> ReadLineFromBuffer_(net::Buffer& buff);

    bool ParseRequestLine_(std::string_view line);
    bool ParseHeader_(std::string_view line);
    void ParseBody_(net::Buffer& buff);

private:
    std::string method_;
    std::string path_;
    std::string version_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;

    bool is_keep_alive_;

    ParseState state_;
    size_t content_len_;
};

}