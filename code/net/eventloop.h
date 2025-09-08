#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>

namespace net {

class Epoller;
class Channel;

class EventLoop {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    void Loop();

    void Quit();

    void RunInLoop(Functor cb);

    void QueueInLoop(Functor cb);

    void UpdateChannel(Channel* channel);

    void AssertInLoopThread();

    bool IsInLoopThread() const { return thread_id_ == std::this_thread::get_id(); }

private:
    void HandleRead_(); // 用于 eventfd
    void Wakeup_();
    void DoPendingFunctors_();

    bool looping_;
    bool quit_;
    bool calling_pending_functors_;
    const std::thread::id thread_id_; // 创建该对象的线程ID

    std::unique_ptr<Epoller> poller_;

    int wakeup_fd_;
    std::unique_ptr<Channel> wakeup_channel_;

    std::mutex mutex_;
    std::vector<Functor> pending_functors_;
};

}