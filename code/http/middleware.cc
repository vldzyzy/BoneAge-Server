#include "middleware.h"
#include <iostream>
#include <string_view>
#include <vector>
#include "logging/logger.h"
#include <string>
#include "inference/boneage_inference.h"
#include "net/eventloop.h"
#include "sql/sqlconnpool.h"

namespace http {
namespace {
std::unordered_map<std::string, std::string> ParseCookies(const HttpRequest& req) {
    std::unordered_map<std::string, std::string> cookies;
    const auto& headers = req.GetHeaders();
    auto it = headers.find("cookie");
    if (it == headers.end()) {
        return cookies;
    }

    std::stringstream ss(it->second);
    std::string segment;

    while(std::getline(ss, segment, ';')) {
        // 去除前导空格
        size_t first = segment.find_first_not_of(" ");
        if (first != std::string::npos) {
            segment = segment.substr(first);
        }

        size_t eq_pos = segment.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = segment.substr(0, eq_pos);
            std::string value = segment.substr(eq_pos + 1);
            cookies[key] = value;
        }
    }
    return cookies;
}

std::string UrlDecode(std::string_view str) {
    std::string decoded;
    decoded.reserve(str.length());
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '%' && i + 2 < str.length()) {
            try {
                std::string hex = std::string(str.substr(i + 1, 2));
                char c = static_cast<char>(std::stoul(hex, nullptr, 16));
                decoded += c;
                i += 2;
            } catch (const std::invalid_argument& e) {
                decoded += str[i];
            }
        } else if (str[i] == '+') {
            decoded += ' ';
        } else {
            decoded += str[i];
        }
    }
    return decoded;
}

void SendResponse(
    const net::TcpConnection::Ptr& conn,
    int status_code,
    std::string status_message,
    std::string body
) {
    http::HttpResponse response; 
    response.SetStatusCode(status_code);
    response.SetStatusMessage(std::move(status_message));
    response.SetBody(std::move(body));
    response.SetContentType("application/json");
    response.SetKeepAlive(true);

    net::Buffer buf;
    response.AppendToBuffer(buf);
    conn->Send(buf);
}
}

void ParseUrlEncoded(HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
    std::string_view path = context.request.GetPath();
    auto query_pos = path.find('?');

    if (query_pos != std::string_view::npos) {
        std::string_view query_string = path.substr(query_pos + 1);

        if (!context.form.has_value()) {
            context.form.emplace();
        }

        size_t start = 0;
        while (start < query_string.length()) {
            size_t next_amp = query_string.find('&', start);
            std::string_view pair_sv = query_string.substr(start, next_amp - start);
            
            size_t eq_pos = pair_sv.find('=');
            if (eq_pos != std::string_view::npos) {
                std::string key = UrlDecode(pair_sv.substr(0, eq_pos));
                std::string value = UrlDecode(pair_sv.substr(eq_pos + 1));

                if (key == "username") {
                    context.form->username = std::move(value);
                } else if (key == "password") {
                    context.form->password = std::move(value);
                }
            }

            if (next_amp == std::string_view::npos) {
                break;
            }
            start = next_amp + 1;
        }
    }
    next();
}

void ParseMultipartForm(HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
    const auto& headers = context.request.GetHeaders();
    auto it = headers.find("content-type");

    if (it != headers.end() && it->second.rfind("multipart/form-data", 0) == 0) {
        std::string boundary_key = "boundary=";
        auto boundary_pos = it->second.find(boundary_key);
        if (boundary_pos == std::string::npos) {
            LOG_ERROR("Malformed multipart: boundary not found");
            next();
            return;
        }
        std::string boundary = "--" + it->second.substr(boundary_pos + boundary_key.length());

        const std::string& body = context.request.GetBody();
        std::string_view body_view(body);

        auto start_pos = body_view.find(boundary);
        if (start_pos == std::string_view::npos) {
            LOG_ERROR("Malformed multipart: malformed body");
            next();
            return;
        }

        std::string_view headers_end_marker = "\r\n\r\n";
        auto headers_end_pos = body_view.find(headers_end_marker, start_pos);
        if (headers_end_pos == std::string_view::npos) {
            LOG_ERROR("Malformed multipart: part header not found");
            next();
            return;
        }

        size_t image_data_start = headers_end_pos + headers_end_marker.length();

        auto end_pos = body_view.find(boundary, image_data_start);
        if (end_pos == std::string_view::npos) {
            LOG_ERROR("Malformed multipart: closing boundary not found");
            next();
            return;
        }
        size_t image_data_end = end_pos - 2;

        if (!context.form) {
            context.form.emplace();
        }
        context.form->image_data = std::vector<unsigned char>(body_view.substr(image_data_start, image_data_end - image_data_start).begin(),
                                                              body_view.substr(image_data_start, image_data_end - image_data_start).end());
    }
    next();
}

void PredictHandler(http::HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
    if (!context.form || !context.form->image_data) {
        context.response.SetStatusCode(400);
        context.response.SetBody("{\"error\": \"Image not found.\"}");
        context.response.SetContentType("application/json");
        net::Buffer buf;
        context.response.AppendToBuffer(buf);
        conn->Send(buf);
        return;
    }
    
    bool keep_alive = context.request.IsKeepAlive();
    
    inference::BoneAgeInferencer::InferenceTask task;
    task.raw_image_data = std::move(*context.form->image_data);
    task.on_complete = [keep_alive, conn](inference::BoneAgeInferencer::InferenceResult result) {
        conn->GetLoop()->RunInLoop([keep_alive, conn = std::move(conn), result = std::move(result)]() {
            if (conn->IsConnected()) {
                http::HttpResponse response;
                response.SetStatusCode(200);
                response.SetBody(std::move(result.result_str));
                response.SetContentType("application/json");
                net::Buffer buf;
                response.AppendToBuffer(buf);
                conn->Send(buf);
            }
        });
    };
    INFERENCER.PostInference(std::move(task));
}

} // namespace http
