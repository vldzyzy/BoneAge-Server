#include "tcpconnection.h"
#include "channel.h"
#include "eventloop.h"
#include "socket.h"
#include <cerrno>
#include <utility>
#include <unistd.h>
#include "logging/logger.h"

namespace net {

TcpConnection::TcpConnection(EventLoop* loop, int sockfd, const std::string& name, const InetAddress& local_addr, const InetAddress& peer_addr)
    : loop_(loop),
      state_(State::kConnecting),
      socket_(std::make_unique<Socket>(sockfd)),
      name_(name),
      channel_(std::make_unique<Channel>(loop, sockfd)),
      local_addr_(local_addr),
      peer_addr_(peer_addr) {

    channel_->SetReadCallback([this] { this->HandleRead_(); });
    channel_->SetWriteCallback([this] { this->HandleWrite_(); });
    channel_->SetCloseCallback([this] { this->HandleClose_(); });
    channel_->SetErrorCallback([this] { this->HandleError_(); });

    socket_->SetKeepAlive(true);
}

TcpConnection::~TcpConnection() {
    LOG_DEBUG("TcpConnection::~TcpConnection() at {} fd={}", static_cast<void*>(this), channel_->GetFd());
}

bool TcpConnection::IsConnected() const {
    return state_ == State::kConnected;
}

void TcpConnection::ConnectEstablished() {
    loop_->AssertInLoopThread();
    SetState_(State::kConnected);
    channel_->Tie(shared_from_this());
    channel_->EnableReading();

    if (connection_callback_) {
        connection_callback_(shared_from_this());
    }
}

void TcpConnection::ConnectDestroyed() {
    loop_->AssertInLoopThread();
    if (state_ == State::kConnected) {
        SetState_(State::kDisconnected);
        channel_->DisableAll();
        if (connection_callback_) {
            connection_callback_(shared_from_this());
        }
    }
    loop_->UpdateChannel(channel_.get());
}

void TcpConnection::HandleRead_() {
    loop_->AssertInLoopThread();
    int saved_errno = 0;
    ssize_t n = input_buffer_.ReadFd(channel_->GetFd(), &saved_errno);

    if (n > 0) {
        // 数据已读入 input_buffer_，直接调用消息回调，让上层处理
        if (message_callback_) {
            message_callback_(shared_from_this(), input_buffer_);
        }
    } else if (n == 0) {
        // 对端关闭连接
        HandleClose_();
    } else {
        errno = saved_errno;
        LOG_ERROR("TcpConnection::handleRead_");
        HandleError_();
    }
}

void TcpConnection::Send(const void* data, size_t len) {
    if (state_ == State::kConnected) {
        if (loop_->IsInLoopThread()) {
            SendInLoop_(data, len);
        } else {
            // Copy data to avoid lifetime issues
            std::string message(static_cast<const char*>(data), len);
            loop_->RunInLoop([ptr = shared_from_this(), msg = std::move(message)]() {
                ptr->SendInLoop_(msg);
            });
        }
    }
}

void TcpConnection::Send(const std::string_view& message) {
    if (state_ == State::kConnected) {
        if (loop_->IsInLoopThread()) {
            SendInLoop_(message);
        } else {
            loop_->RunInLoop([ptr = shared_from_this(), msg = std::string(message)]() {
                ptr->SendInLoop_(msg);
            });
        }
    }
}

void TcpConnection::Send(Buffer& buf) {
    if (state_ == State::kConnected) {
        if (loop_->IsInLoopThread()) {
            SendInLoop_(buf.Peek(), buf.ReadableBytes());
            buf.RetrieveAll();
        } else {
            loop_->RunInLoop([ptr = shared_from_this(), msg = buf.RetrieveAllToString()]() {
                ptr->SendInLoop_(msg);
            });
        }
    }
}

void TcpConnection::SendInLoop_(const std::string_view& message) {
    SendInLoop_(message.data(), message.size());
}

void TcpConnection::SendInLoop_(const void* data, size_t len) {
    loop_->AssertInLoopThread();
    ssize_t nwrote = 0;
    size_t remaining = len;
    bool fault_error = false;

    // 如果输出缓冲区为空，尝试直接发送
    if (!channel_->IsWriting() && output_buffer_.ReadableBytes() == 0) {
        nwrote = ::write(channel_->GetFd(), data, len);
        if (nwrote >= 0) {
            remaining = len - nwrote;
            if (remaining == 0 && write_complete_callback_) {
                // 数据一次性发送完毕，调用写完成回调
                loop_->QueueInLoop([ptr = shared_from_this()]() {
                    ptr->write_complete_callback_(ptr);
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK) {
                LOG_ERROR("TcpConnection::sendInLoop_");
                if (errno == EPIPE || errno == ECONNRESET) {
                    fault_error = true;
                }
            }
        }
    }

    // 如果数据没有一次性发送完，放入输出缓冲区，并开始关注写事件
    if (!fault_error && remaining > 0) {
        output_buffer_.Append(static_cast<const char*>(data) + nwrote, remaining);
        if (!channel_->IsWriting()) {
            channel_->EnableWriting();
        }
    }
}

void TcpConnection::HandleWrite_() {
    loop_->AssertInLoopThread();
    if (channel_->IsWriting()) {
        ssize_t n = ::write(channel_->GetFd(), output_buffer_.Peek(), output_buffer_.ReadableBytes());
        if (n > 0) {
            output_buffer_.Retrieve(n);
            if (output_buffer_.ReadableBytes() == 0) {
                channel_->DisableWriting();
                if (write_complete_callback_) {
                     loop_->QueueInLoop([ptr = shared_from_this()]() {
                        ptr->write_complete_callback_(ptr);
                    });
                }
                if (state_ == State::kDisconnecting) {
                    ShutdownInLoop_();
                }
            }
        } else {
            LOG_ERROR("TcpConnection::handleWrite_");
        }
    }
}

void TcpConnection::Shutdown() {
    if (state_ == State::kConnected) {
        SetState_(State::kDisconnecting);
        loop_->RunInLoop([ptr = shared_from_this()] {
            ptr->ShutdownInLoop_();
        });
    }
}

void TcpConnection::ShutdownInLoop_() {
    loop_->AssertInLoopThread();
    if (!channel_->IsWriting()) { // 只有当没有数据待发送时才关闭写端
        socket_->ShutdownWrite();
    }
}

void TcpConnection::HandleClose_() {
    loop_->AssertInLoopThread();
    SetState_(State::kDisconnected);
    channel_->DisableAll();

    TcpConnection::Ptr guard_this(shared_from_this());
    if (connection_callback_) {
        connection_callback_(guard_this); 
    }
    if (close_callback_) {
        // close_callback_ 负责从 Server 中移除此 TcpConnection
        close_callback_(guard_this);
    }
}

void TcpConnection::HandleError_() {
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if (::getsockopt(channel_->GetFd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0) {
        err = errno;
    } else {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError_ state={}, err={}", static_cast<int>(state_), err);
}

} // namespace net
