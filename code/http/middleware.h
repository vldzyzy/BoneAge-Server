#pragma once

#include "httpcontext.h"
#include "net/tcpconnection.h"

namespace http {



using Next = std::function<void()>;
using Middleware = std::function<void(HttpContext& context, 
                                     const net::TcpConnection::Ptr& conn, 
                                     const Next& next)>;

void ParseUrlEncoded(HttpContext& context, 
                                     const net::TcpConnection::Ptr& conn, 
                                     const Next& next);

void ParseMultipartForm(HttpContext& context, 
                                     const net::TcpConnection::Ptr& conn, 
                                     const Next& next);


void PredictHandler(HttpContext& context, 
                                     const net::TcpConnection::Ptr& conn, 
                                     const Next& next);

} // namespace http
