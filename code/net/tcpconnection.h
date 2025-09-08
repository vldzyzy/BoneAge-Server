#pragma once

#include "buffer.h"
#include "inetaddress.h"
#include <memory>
#include <functional>
#include <string_view>
#include <any>

namespace net {

class Channel;
class EventLoop;
class Socket;
class Buffer;

class TcpConnection : public std::enable_shared_from_this<TcpConnection> {
public:
    using Ptr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const Ptr&)>;
    using MessageCallback = std::function<void(const Ptr&, Buffer&)>;
    using CloseCallback = std::function<void(const Ptr&)>;
    using WriteCompleteCallback = std::function<void(const Ptr&)>;

    TcpConnection(EventLoop* loop, int sockfd, const std::string& name, const InetAddress& local_addr, const InetAddress& peer_addr);
    ~TcpConnection();

    void Send(const void* data, size_t len);
    void Send(const std::string_view& message);
    void Send(Buffer& buf);
    void Shutdown();

    // 通过回调添加到server中之后调用
    void ConnectEstablished();
    // 从server中删除后调用
    void ConnectDestroyed();

    bool IsConnected() const;
    EventLoop* GetLoop() const { return loop_; }
    const std::string& GetName() const { return name_; }
    const InetAddress& GetLocalAddress() const { return local_addr_; }
    const InetAddress& GetPeerAddress() const { return peer_addr_; }

    void SetContext(const std::any& context) { context_ = context; }
    const std::any& GetContext() const { return context_; }
    std::any* GetMutableContext() { return &context_; }

    void SetConnectionCallback(const ConnectionCallback& cb) { connection_callback_ = cb; }
    void SetMessageCallback(const MessageCallback& cb) { message_callback_ = cb; }
    void SetCloseCallback(const CloseCallback& cb) { close_callback_ = cb; }
    void SetWriteCompleteCallback(const WriteCompleteCallback& cb) { write_complete_callback_ = cb; }

private:
    enum class State { kConnecting, kConnected, kDisconnecting, kDisconnected };

    void SetState_(State s) { state_ = s; }

    // Channel 回调
    void HandleRead_();
    void HandleWrite_();
    void HandleClose_();
    void HandleError_();

    // 运行在 IO 线程中的方法
    void SendInLoop_(const std::string_view& message);
    void SendInLoop_(const void* data, size_t len);
    void ShutdownInLoop_();

    EventLoop* loop_;
    const std::string name_;
    State state_;
    
    std::unique_ptr<Socket> socket_;
    std::unique_ptr<Channel> channel_;

    const InetAddress local_addr_;
    const InetAddress peer_addr_;

    ConnectionCallback connection_callback_;
    MessageCallback message_callback_;
    CloseCallback close_callback_;
    WriteCompleteCallback write_complete_callback_;

    Buffer input_buffer_;
    Buffer output_buffer_;

    std::any context_;
};

} // namespace net
