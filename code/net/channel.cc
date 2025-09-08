#include "channel.h"
#include "eventloop.h"
#include <cassert>

namespace net {

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      is_in_epoll_(false),
      tied_(false) {}

void Channel::HandleEvent() {
    if (tied_) {
        std::shared_ptr<void> guard = tie_.lock();
        if (guard) {
            HandleEventWithGuard_();
        }
    } else {
        HandleEventWithGuard_();
    }
}

void Channel::HandleEventWithGuard_() {
    // 对端关闭连接
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (close_callback_) close_callback_();
    }
    // 发生错误
    if (revents_ & EPOLLERR) {
        if (error_callback_) error_callback_();
    }
    // 可读事件或紧急数据
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (read_callback_) read_callback_();
    }
    // 可写事件
    if (revents_ & EPOLLOUT) {
        if (write_callback_) write_callback_();
    }
}

void Channel::Tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}

void Channel::EnableReading() {
    events_ |= kReadEvent;
    Update_();
}

void Channel::DisableReading() {
    events_ &= ~kReadEvent;
    Update_();
}

void Channel::EnableWriting() {
    events_ |= kWriteEvent;
    Update_();
}

void Channel::DisableWriting() {
    events_ &= ~kWriteEvent;
    Update_();
}

void Channel::DisableAll() {
    events_ = kNoneEvent;
    Update_();
}

void Channel::Update_() {
    loop_->UpdateChannel(this);
}

}