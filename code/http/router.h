#pragma once

#include "httpcontext.h"
#include <functional>
#include <string>
#include <unordered_map>
#include "net/tcpconnection.h"
#include <initializer_list>
#include "middleware.h"

namespace http {

class Router {
public:
    Router();

    void AddRoute(std::string method, std::string path, std::initializer_list<Middleware> middlewares);
    void Route(HttpContext& context, const net::TcpConnection::Ptr& conn);

private:
    std::unordered_map<std::string, std::vector<Middleware>> routes_;
    Middleware not_found_handler_;
};

} // namespace http
