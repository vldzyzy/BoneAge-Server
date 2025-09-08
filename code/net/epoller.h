#pragma once

#include <vector>
#include <map>
#include <sys/epoll.h>

namespace net {

class Channel;

class Epoller {
public:
    using ChannelList = std::vector<Channel*>;

    Epoller();
    ~Epoller();

    Epoller(const Epoller&) = delete;
    Epoller& operator=(const Epoller&) = delete;

    // 轮询 I/O 事件，将活跃的 Channel 填充到 active_channels 中
    void Poll(int timeout_ms, ChannelList* active_channels);

    // 添加/修改/删除
    void UpdateChannel(Channel* channel);

private:
    void Update_(int operation, Channel* channel);

private:
    using EventList = std::vector<struct epoll_event>;
    using ChannelMap = std::map<int, Channel*>; // fd -> Channel*

    int epoll_fd_;
    EventList events_;
    ChannelMap channels_;

    static constexpr int kInitEventListSize = 16;
};

} // namespace net
