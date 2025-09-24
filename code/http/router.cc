#include "router.h"
#include "logging/logger.h"

namespace http {

Router::Router() {
  not_found_handler_ = [](HttpContext &context,
                          const net::TcpConnection::Ptr &conn,
                          const Next &next) {
    context.response.SetStatusCode(404);
    context.response.SetStatusMessage("Not Found");
    context.response.SetBody("404 Not Found");
    net::Buffer buf;
    context.response.AppendToBuffer(buf);
    auto str = std::string(buf.Peek(), buf.ReadableBytes());
    conn->Send(buf);
  };
}

void Router::AddRoute(std::string method, std::string path,
                      std::initializer_list<Middleware> middlewares) {
  std::string key = std::move(method) + ":" + std::move(path);
  routes_[key] = middlewares;
}

void Router::Route(HttpContext &context, const net::TcpConnection::Ptr &conn) {
  std::string key =
      context.request.GetMethod() + ":" + context.request.GetPath();

  auto it = routes_.find(key);
  const auto &chain = (it != routes_.end())
                          ? it->second
                          : std::vector<Middleware>{not_found_handler_};

  size_t index = 0;
  std::function<void()> next_dispatcher = [&]() {
    if (index < chain.size()) {
      const auto &current_middleware = chain[index];
      index++;
      current_middleware(context, conn, next_dispatcher);
    }
  };

  next_dispatcher();
}

} // namespace http
