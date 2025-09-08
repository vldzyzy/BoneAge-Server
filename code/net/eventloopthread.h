#pragma once

#include <condition_variable>
#include <thread>

namespace net {

class EventLoop;

class EventLoopThread {
public:
    EventLoopThread() = default;
    ~EventLoopThread();

    EventLoopThread(const EventLoopThread&) = delete;
    EventLoopThread& operator=(const EventLoopThread&) = delete;

    EventLoop* StartLoop();

private:
    void Run_();

private:
    EventLoop* loop_{nullptr};
    
    std::thread loop_thread_;

    std::mutex mutex_;
    std::condition_variable loop_cv_;
};

}