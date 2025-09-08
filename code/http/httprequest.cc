#include "httprequest.h"
#include "logging/logger.h"
#include <algorithm>
#include <cctype>

namespace http {

using namespace net;

void HttpRequest::Reset() {
    method_ = {};
    path_ = {};
    version_ = {};
    headers_.clear();
    body_.clear();
    is_keep_alive_ = false;
    state_ = ParseState::kRequestLine;
    content_len_ = 0;
}

std::optional<std::string_view> HttpRequest::ReadLineFromBuffer_(Buffer& buff) {
    const char* crlf = "\r\n";
    const char* line_end = std::search(buff.Peek(), buff.BeginWriteConst(), crlf, crlf + 2);
    if (line_end == buff.BeginWriteConst()) {
        return std::nullopt;
    }
    std::string_view line(buff.Peek(), line_end - buff.Peek()); // line 不包含 crlf
    buff.Retrieve(line.size() + 2);
    return line;
}

HttpRequest::HttpCode HttpRequest::Parse(Buffer& buff) {
    while (state_ != ParseState::kFinish) {
        if (state_ == ParseState::kBody) {
            if (buff.ReadableBytes() == 0 && body_.size() < content_len_) {
                return HttpCode::kNoRequest;
            }
            if (body_.size() == content_len_) {
                state_ = ParseState::kFinish;
                break;
            }
            size_t bytes_to_read = buff.ReadableBytes();
            body_.append(buff.Peek(), bytes_to_read);
            buff.Retrieve(bytes_to_read);

            continue;
        }
    
        auto line = ReadLineFromBuffer_(buff);
        if (!line) {
            return HttpCode::kNoRequest; // 数据不足, 接着进行下一次读取
        }

        switch (state_) {
            case ParseState::kRequestLine:
                if (!ParseRequestLine_(*line)) {
                    return HttpCode::kBadRequest;
                }
                state_ = ParseState::kHeaders;
                break;
            case ParseState::kHeaders:
                if (line->empty()) { // 空行, headers结束
                    state_ = (content_len_ > 0) ? ParseState::kBody : ParseState::kFinish;
                } else if (!ParseHeader_(*line)) {
                    return HttpCode::kBadRequest;
                }
                break;
            default:
                state_ = ParseState::kFinish;
                break;
        }
    }

    auto it = headers_.find("connection");
    if (it != headers_.end()) {
        std::string value = it->second;
        if (value == "keep-alive") {
            is_keep_alive_ = true;
        } else {
            is_keep_alive_ = false;
        }
    } else {
        is_keep_alive_ = (version_ == "HTTP/1.1");
    }
    return HttpCode::kGetRequest;
}

bool HttpRequest::ParseRequestLine_(std::string_view line) {
    size_t method_end = line.find(' ');
    if (method_end == std::string_view::npos) {
        return false;
    }
    method_ = line.substr(0, method_end);

    size_t path_end = line.find(' ', method_end + 1);
    if (path_end == std::string_view::npos) {
        return false;
    }
    path_ = line.substr(method_end + 1, path_end - (method_end + 1));
    version_ = line.substr(path_end + 1);

    return !method_.empty() && !path_.empty() && (version_ == "HTTP/1.1" || version_ == "HTTP/1.0");
}

bool HttpRequest::ParseHeader_(std::string_view line) {
    size_t colon_pos = line.find(':');
    if (colon_pos == std::string_view::npos) {
        return false;
    }

    std::string_view key_sv = line.substr(0, colon_pos);
    std::string_view value_sv = line.substr(colon_pos + 1);

    value_sv.remove_prefix(std::min(value_sv.find_first_not_of(" \t"), value_sv.size()));
    value_sv.remove_suffix(std::min(value_sv.size() - value_sv.find_last_not_of(" \t") - 1, value_sv.size()));

    std::string key;
    key.resize(key_sv.size());
    std::transform(key_sv.begin(), key_sv.end(), key.begin(), tolower);

    headers_[key] = std::string(value_sv);

    if (key == "content-length") {
        content_len_ = std::stoul(std::string(value_sv));
    }
    return true;
}

}