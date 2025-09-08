#include "epoller.h"
#include "channel.h"
#include <unistd.h>
#include <stdexcept>
#include <cassert>
#include <cerrno>
#include "logging/logger.h"

namespace net {

Epoller::Epoller()
    : epoll_fd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    if (epoll_fd_ < 0) {
        throw std::runtime_error("Epoller::epoll_create1() failed.");
    }
}

Epoller::~Epoller() {
    ::close(epoll_fd_);
}

void Epoller::Poll(int timeout_ms, ChannelList* active_channels) {
    int num_events = ::epoll_wait(
        epoll_fd_,
        events_.data(),
        static_cast<int>(events_.size()),
        timeout_ms);
    
    if (num_events > 0) {
        for (int i = 0; i < num_events; ++i) {
            // 从 epoll_event.data.ptr 中直接取出 Channel 指针
            Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
            channel->SetRevents(events_[i].events);
            active_channels->push_back(channel);
        }
        // 如果事件数组满了，进行扩容
        if (static_cast<size_t>(num_events) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (num_events < 0 && errno != EINTR) {
        LOG_ERROR("Epoller::Poll() error");
    }
}

void Epoller::UpdateChannel(Channel* channel) {
    const int fd = channel->GetFd();

    if (channel->IsNoneEvent()) {
        // DEL
        if (channels_.count(fd)) {
            Update_(EPOLL_CTL_DEL, channel);
            channels_.erase(fd);
        }
    } else {
        if (channels_.find(fd) == channels_.end()) {
            // ADD
            channels_[fd] = channel;
            Update_(EPOLL_CTL_ADD, channel);
        } else {
            // MOD
            Update_(EPOLL_CTL_MOD, channel);
        }
    }
}

void Epoller::Update_(int operation, Channel* channel) {
    struct epoll_event event;
    event.events = channel->GetEvents();
    event.data.ptr = channel;
    const int fd = channel->GetFd();

    if (::epoll_ctl(epoll_fd_, operation, fd, &event) < 0) {
        LOG_ERROR("epoll_ctl op={} fd={}", operation, fd);
    }
}

} // namespace net
