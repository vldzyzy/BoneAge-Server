#pragma once

#include "httprequest.h"
#include "httpresponse.h"
#include <any>
#include <unordered_map>
#include <optional>

namespace http {

struct ParsedForm {
    std::optional<std::vector<unsigned char>> image_data;
    std::optional<std::string> username;
    std::optional<std::string> password;
};

// 存储一次HTTP请求的完整上下文
struct HttpContext {
    HttpRequest request;
    HttpResponse response;

    std::optional<ParsedForm> form;
    std::optional<std::string> authenticated_user;

    void Reset() {
        request.Reset();
        response.Reset();
        form.reset();
        authenticated_user.reset();
    }
};

} // namespace net
