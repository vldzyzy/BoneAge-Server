#include "eventloopthread.h"
#include "eventloop.h"
#include <mutex>

namespace net {

EventLoopThread::~EventLoopThread() {
    if (loop_ != nullptr) {
        loop_->Quit();
    }
    if (loop_thread_.joinable()) {
        loop_thread_.join();
    }
}

EventLoop* EventLoopThread::StartLoop() {
    loop_thread_ = std::thread([this] { Run_(); });
    
    std::unique_lock<std::mutex> lock(mutex_);
    loop_cv_.wait(lock, [this]() { return loop_ != nullptr; });
    return loop_;
}

void EventLoopThread::Run_() {
    EventLoop loop;

    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = &loop;
    lock.unlock();

    loop_cv_.notify_one();

    loop.Loop();

    lock.lock();
    loop_ = nullptr;
    return;
}

}