#include "eventloop.h"
#include "epoller.h"
#include "channel.h"
#include <cassert>
#include <memory>
#include <stdexcept>
#include <sys/eventfd.h>
#include <sys/types.h>
#include <unistd.h>
#include "logging/logger.h"

namespace net {

namespace {
// 创建eventfd，用于线程间通信
int CreateEventFd() {
    int evtfd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC); // 信号模式
    if (evtfd < 0) {
        throw std::runtime_error("CreateEventFd failed");
    }
    return evtfd;
}
}

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      thread_id_(std::this_thread::get_id()),
      poller_(std::make_unique<Epoller>()),
      wakeup_fd_(CreateEventFd()),
      wakeup_channel_(std::make_unique<Channel>(this, wakeup_fd_)) {
    
    wakeup_channel_->SetReadCallback([this] { this->HandleRead_(); });
    wakeup_channel_->EnableReading();
}

EventLoop::~EventLoop() {
    wakeup_channel_->DisableAll();
    UpdateChannel(wakeup_channel_.get());
    ::close(wakeup_fd_);
}

void EventLoop::Loop() {
    AssertInLoopThread();
    looping_ = true;
    quit_ = false;

    Epoller::ChannelList active_channels;

    while (!quit_) {
        active_channels.clear();
        poller_->Poll(-1, &active_channels);

        for (Channel* channel : active_channels) {
            channel->HandleEvent();
        }

        DoPendingFunctors_();
    }
    looping_ = false;
}

void EventLoop::Quit() {
    quit_ = true;
    // 如果是在其他线程调用 Quit，需要唤醒 loop
    if (!IsInLoopThread()) {
        Wakeup_();
    }
}

void EventLoop::RunInLoop(Functor cb) {
    if (IsInLoopThread()) {
        cb();
    } else {
        QueueInLoop(std::move(cb));
    }
}

void EventLoop::QueueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pending_functors_.push_back(std::move(cb));
    }

    if (!IsInLoopThread() || calling_pending_functors_) {
        Wakeup_();
    }
}

void EventLoop::UpdateChannel(Channel* channel) {
    AssertInLoopThread();
    poller_->UpdateChannel(channel);
}

void EventLoop::HandleRead_() {
    uint64_t one = 1;
    ssize_t n = ::read(wakeup_fd_, &one, sizeof(one));
    assert(n == sizeof(one));
}

void EventLoop::Wakeup_() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeup_fd_, &one, sizeof(one));
    assert(n == sizeof(one));
}

void EventLoop::DoPendingFunctors_() {
    std::vector<Functor> functors;
    calling_pending_functors_ = true;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pending_functors_);
    }

    for (const Functor& functor : functors) {
        functor();
    }
    calling_pending_functors_ = false;
}

void EventLoop::AssertInLoopThread() {
    assert(IsInLoopThread());
}

}