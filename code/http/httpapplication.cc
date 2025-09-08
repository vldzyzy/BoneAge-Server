#include "context/context.h"
#include "http/httpcontext.h"
#include "http/httprequest.h"
#include "httpapplication.h"
#include "logging/logger.h"
#include "inference/boneage_inference.h"
#include "net/eventloop.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <chrono>

namespace http {

namespace {
std::string GetMimeType(const std::string& path) {
    static const std::unordered_map<std::string, std::string> mime_map = {
        {".html", "text/html; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".js",   "application/javascript; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".ico",  "image/x-icon"},
        {".svg",  "image/svg+xml"}
    };
    
    std::string_view p(path);
    auto dot_pos = p.find_last_of('.');
    if (dot_pos == std::string_view::npos) {
        return "application/octet-stream";
    }
    
    std::string ext(p.substr(dot_pos));
    
    auto it = mime_map.find(ext);
    return (it != mime_map.end()) ? it->second : "application/octet-stream";
}
} // namespace

using namespace net;

HttpApplication::HttpApplication(InetAddress listen_addr, std::string static_root_dir, std::string name, const int workers_num)
    : server_(std::move(listen_addr), std::move(name))
    , static_root_dir_(static_root_dir) {

    server_.SetConnectionCallback(
        [this](const TcpConnection::Ptr& conn) {
            this->OnConnection_(conn);
        }
    );
    
    server_.SetMessageCallback(
        [this](const TcpConnection::Ptr& conn, Buffer& buf) {
            this->OnMessage_(conn, buf);
        }
    );

    // task_runner_ = NEW_PARALLEL_RUNNER(2, workers_num);

    if (std::filesystem::exists(static_root_dir_)) {
        CacheStaticFiles_(static_root_dir_);
    } else {
        LOG_WARN("Static path {} does not exist.", static_root_dir_);
    }

    router_.AddRoute("POST", "/predict", {
        [this](auto& context, auto& conn, auto& next) {
            this->ParseMultipartForm_(context, conn, next);
        },
        [this](auto& context, auto& conn, auto& next) {
            this->PredictHandler_(context, conn, next);
        }
    });

    for (const auto& [web_path, content] : static_file_cache_) {
        router_.AddRoute("GET", web_path, {
            [this](auto& context, auto& conn, auto& next) {
                this->StaticFileHandler_(context, conn, next);
            }
        });
        LOG_INFO("Added static route: GET {}", web_path);
    }
}

void HttpApplication::Start() {
    server_.Start();
}

void HttpApplication::OnConnection_(const net::TcpConnection::Ptr& conn) {
    if (conn->IsConnected()) {
        conn->SetContext(HttpContext());
        LOG_INFO("client {} connected", conn->GetName());
    } else {
        LOG_INFO("client {} quit", conn->GetName());
    }
}

void HttpApplication::OnMessage_(const TcpConnection::Ptr& conn, Buffer& buf) {
    HttpContext* context = std::any_cast<HttpContext>(conn->GetMutableContext());

    while (buf.ReadableBytes() > 0) {
        HttpRequest::HttpCode result = context->request.Parse(buf);

        if (result == HttpRequest::HttpCode::kGetRequest) {
            LOG_INFO(context->request.GetPath());
            context->response.SetKeepAlive(context->request.IsKeepAlive());
            // LOG_INFO("Is keep-alive: {}", context->response.IsKeepAlive());
            // LOG_INFO("HTTP version: {}", context->request.GetVersion());
            router_.Route(*context, conn);

            if (context->response.IsKeepAlive()) {
                context->Reset();
            } else {
                conn->Shutdown();
                break; 
            }
        } else if (result == HttpRequest::HttpCode::kBadRequest) {
            conn->Send("HTTP/1.1 400 Bad Request\r\n\r\n");
            conn->Shutdown();
            break;
        } else { // kNoRequest
            break;
        }
    }
}

void HttpApplication::ParseMultipartForm_(HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
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

void HttpApplication::PredictHandler_(http::HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
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

void HttpApplication::CacheStaticFile_(const std::string& file_path) {
    std::string web_path = file_path.substr(static_root_dir_.length());
    if (web_path.empty()) web_path = "/";

    // 获取文件修改时间
    std::filesystem::file_time_type ftime = std::filesystem::last_write_time(file_path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t mod_time = std::chrono::system_clock::to_time_t(sctp);

    std::ifstream file(file_path, std::ios::binary);
    if (file) {
        std::ostringstream ss;
        ss << file.rdbuf();
        static_file_cache_[web_path] = ss.str();
        file_mod_times_[web_path] = mod_time;
        LOG_INFO("Cached static file: {} -> {} ({} bytes)", file_path, web_path, static_file_cache_[web_path].size());

        if (web_path == "/index.html") {
            static_file_cache_["/"] = static_file_cache_[web_path];
            file_mod_times_["/"] = mod_time;
        }
    }
}

void HttpApplication::CacheStaticFiles_(const std::string& path) {
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
            CacheStaticFile_(entry.path().string());
        }
    }
}

void HttpApplication::StaticFileHandler_(HttpContext& context, const net::TcpConnection::Ptr& conn, const Next& next) {
    std::string path = context.request.GetPath();
    if (path.empty() || path == "/") {
        path = "/index.html";
    }

    // 构建实际文件路径
    std::string file_path = static_root_dir_ + path;
    if (path == "/index.html") {
        file_path = static_root_dir_ + "/index.html";
    }

    // 检查文件是否存在
    if (!std::filesystem::exists(file_path)) {
        context.response.SetStatusCode(404);
        context.response.SetStatusMessage("Not Found");
        context.response.SetContentType("text/plain; charset=utf-8");
        context.response.SetBody("404 Not Found: The requested resource '" + path + "' does not exist.");
        
        net::Buffer buf;
        context.response.AppendToBuffer(buf);
        conn->Send(buf);
        return;
    }

    // 获取文件当前修改时间
    std::filesystem::file_time_type ftime = std::filesystem::last_write_time(file_path);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    std::time_t current_mod_time = std::chrono::system_clock::to_time_t(sctp);

    // 检查缓存是否存在且是否需要更新
    auto cache_it = static_file_cache_.find(path);
    auto time_it = file_mod_times_.find(path);
    
    bool need_update = (cache_it == static_file_cache_.end() || 
                       time_it == file_mod_times_.end() || 
                       time_it->second < current_mod_time);

    if (need_update) {
        // 重新缓存文件
        CacheStaticFile_(file_path);
        LOG_INFO("File updated, re-cached: {}", file_path);
    }

    // 从缓存中获取文件内容
    cache_it = static_file_cache_.find(path);
    if (cache_it != static_file_cache_.end()) {
        context.response.SetStatusCode(200);
        context.response.SetStatusMessage("OK");
        context.response.SetContentType(GetMimeType(path));
        context.response.SetBody(cache_it->second);
        
        net::Buffer buf;
        context.response.AppendToBuffer(buf);
        conn->Send(buf);
    } else {
        context.response.SetStatusCode(500);
        context.response.SetStatusMessage("Internal Server Error");
        context.response.SetContentType("text/plain; charset=utf-8");
        context.response.SetBody("500 Internal Server Error: Failed to load file.");
        
        net::Buffer buf;
        context.response.AppendToBuffer(buf);
        conn->Send(buf);
    }
}

} // namespace net
