#include "httpresponse.h"
#include <iostream>

namespace http {

using namespace net;

static const std::unordered_map<int, std::string> kStatusCodeToString = {
    {200, "OK"},        {400, "Bad Request"},           {403, "Forbidden"},
    {404, "Not Found"}, {500, "Internal Server Error"},
};

void HttpResponse::Reset() {
  status_code_ = 200; // 默认成功
  status_message_.clear();
  is_keep_alive_ = false;
  headers_.clear();
  body_.clear();
}

void HttpResponse::SetStatusCode(int code) { status_code_ = code; }

void HttpResponse::SetStatusMessage(std::string message) {
  status_message_ = std::move(message);
}

void HttpResponse::SetKeepAlive(bool on) { is_keep_alive_ = on; }

void HttpResponse::SetContentType(std::string content_type) {
  SetHeader("Content-Type", std::move(content_type));
}

void HttpResponse::SetHeader(std::string key, std::string value) {
  headers_.emplace(std::move(key), std::move(value));
}

void HttpResponse::SetBody(std::string body) { body_ = std::move(body); }

void HttpResponse::AppendToBuffer(Buffer &buffer) {
  headers_["Content-Length"] = std::to_string(body_.size());
  if (is_keep_alive_) {
    headers_["Connection"] = "keep-alive";
  } else {
    headers_["Connection"] = "close";
  }

  buffer.Append("HTTP/1.1 " + std::to_string(status_code_) + " ");
  if (!status_message_.empty()) {
    buffer.Append(status_message_);
  } else {
    auto it = kStatusCodeToString.find(status_code_);
    if (it != kStatusCodeToString.end()) {
      buffer.Append(it->second);
    }
  }
  buffer.Append("\r\n");

  for (const auto &header : headers_) {
    buffer.Append(header.first);
    buffer.Append(": ");
    buffer.Append(header.second);
    buffer.Append("\r\n");
  }

  buffer.Append("\r\n");
  if (!body_.empty()) {
    buffer.Append(body_);
  }
}
} // namespace http