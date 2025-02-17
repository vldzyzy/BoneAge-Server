#pragma once
#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <vector>
#include <errno.h>

class Epoller {
public:
    explicit Epoller(int maxEvent = 1024);

    ~Epoller();

    // 向epoll中添加文件描述符
    bool addFd(int fd, uint32_t events);

    // 修改文件描述符的事件类型
    bool modFd(int fd, uint32_t events);

    // 从epoll中删除文件描述符
    bool delFd(int fd);

    // 等待并返回就绪事件数量
    int wait(int timeoutMs = -1);

    // 获取某个事件的文件描述符
    int getEventFd(size_t i) const;

    // 获取某个事件的事件类型
    uint32_t getEvents(size_t i) const;

private:
    int _epollFd;   // epoll实例的文件描述符
    std::vector<epoll_event> _events;    // 存储事件的数组
};