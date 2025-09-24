#pragma once

#include "context/context.h"
#include "context/executor.h"
#include "httpcontext.h"
#include "net/tcpconnection.h"
#include "net/tcpserver.h"
#include "router.h"
#include <string>
#include <unordered_map>

namespace http {

class HttpApplication {
public:
  HttpApplication(net::InetAddress listen_addr, std::string static_root_dir,
                  std::string name, const int workers_num = 4);

  void SetThreadNum(int num_threads) { server_.SetThreadNum(num_threads); }
  void Start();

private:
  void OnConnection_(const net::TcpConnection::Ptr &conn);
  void OnMessage_(const net::TcpConnection::Ptr &conn, net::Buffer &buf);

  void ParseMultipartForm_(HttpContext &context,
                           const net::TcpConnection::Ptr &conn,
                           const Next &next);
  void PredictHandler_(HttpContext &context,
                       const net::TcpConnection::Ptr &conn, const Next &next);
  void StaticFileHandler_(HttpContext &context,
                          const net::TcpConnection::Ptr &conn,
                          const Next &next);

  void CacheStaticFile_(const std::string &file_path);
  void CacheStaticFiles_(const std::string &path);

  net::TcpServer server_;
  Router router_;

  ctx::TaskRunnerTag task_runner_;

  std::string static_root_dir_;
  std::unordered_map<std::string, std::string> static_file_cache_;
  std::unordered_map<std::string, std::time_t> file_mod_times_;
};

} // namespace http
