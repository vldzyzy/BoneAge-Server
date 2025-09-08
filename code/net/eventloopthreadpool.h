#pragma once

#include <vector>
#include <memory>
#include <atomic>

namespace net {

class EventLoop;

class EventLoopThread;

class EventLoopThreadPool {
public:
    explicit EventLoopThreadPool(int loop_count);
    ~EventLoopThreadPool();

    EventLoopThreadPool(const EventLoopThreadPool&) = delete;
    EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

    void Start();
    EventLoop* GetNextLoop();

private:
    int loop_count_;
    std::vector<std::unique_ptr<EventLoopThread>> loop_threads_;
    std::vector<EventLoop*> loops_;
    std::atomic<int> next_{0};
};

}