#pragma once

#include <functional>
#include <sys/epoll.h>
#include <memory>

namespace net {

class EventLoop;

class Channel {
public:
    using EventCallback = std::function<void()>;

    Channel(EventLoop* loop, int fd);
    ~Channel() = default;

    Channel(const Channel&) = delete;
    Channel& operator=(const Channel&) = delete;

    void HandleEvent();

    void Tie(const std::shared_ptr<void>& obj);

    void SetReadCallback(EventCallback cb) { read_callback_ = std::move(cb); }
    void SetWriteCallback(EventCallback cb) { write_callback_ = std::move(cb); }
    void SetCloseCallback(EventCallback cb) { close_callback_ = std::move(cb); }
    void SetErrorCallback(EventCallback cb) { error_callback_ = std::move(cb); }

    void EnableReading();
    void DisableReading();
    void EnableWriting();
    void DisableWriting();
    void DisableAll();

    bool IsWriting() const { return events_ & kWriteEvent; }
    bool IsReading() const { return events_ & kReadEvent; }
    bool IsNoneEvent() const { return events_ == kNoneEvent; }

    int GetFd() const { return fd_; }
    uint32_t GetEvents() const { return events_; }

    void SetRevents(uint32_t revt) { revents_ = revt; }

private:
    void Update_();
    void HandleEventWithGuard_();

private:
    static constexpr uint32_t kNoneEvent = 0;
    static constexpr uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
    static constexpr uint32_t kWriteEvent = EPOLLOUT;

    EventLoop* loop_;
    const int fd_;

    uint32_t events_;  // 关注的事件
    uint32_t revents_; // epoll 返回的就绪事件
    bool is_in_epoll_;

    std::weak_ptr<void> tie_;
    bool tied_;

    EventCallback read_callback_;
    EventCallback write_callback_;
    EventCallback close_callback_;
    EventCallback error_callback_;
};

}