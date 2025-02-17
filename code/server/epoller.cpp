#include "epoller.h"

Epoller::Epoller(int maxEvents)
    : _epollFd(epoll_create(512))
    , _events(maxEvents) {
        // 检查epollFd是否创建成功，且事件数组大小大于0
        assert(_epollFd >= 0 && _events.size() > 0);
    }

Epoller::~Epoller() {
    close(_epollFd);
}


bool Epoller::addFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    // 成功返回true, 失败返回false
    return 0 == epoll_ctl(_epollFd, EPOLL_CTL_ADD, fd, &ev);
}

bool Epoller::modFd(int fd, uint32_t events) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    ev.data.fd = fd;
    ev.events = events;

    return 0 == epoll_ctl(_epollFd, EPOLL_CTL_MOD, fd, &ev);
}

bool Epoller::delFd(int fd) {
    if(fd < 0) return false;
    epoll_event ev = {0};
    return 0 == epoll_ctl(_epollFd, EPOLL_CTL_DEL, fd, &ev);
}

// timeroutMs: 等待的超时时间，单位毫秒，-1表示无限等待
// 返回：就绪事件的数量
int Epoller::wait(int timeoutMs) {
    return epoll_wait(_epollFd, &_events[0], static_cast<int>(_events.size()), timeoutMs);
}

int Epoller::getEventFd(size_t i) const {
    assert(i < _events.size() && i >= 0);
    return _events[i].data.fd;
}

// i: 事件的索引
// 返回： 事件类型
uint32_t Epoller::getEvents(size_t i) const {
    assert(i < _events.size() && i >= 0);
    return _events[i].events;
}