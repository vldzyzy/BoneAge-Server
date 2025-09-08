#include "eventloopthreadpool.h"
#include "eventloopthread.h"
#include "net/channel.h"

namespace net {

EventLoopThreadPool::EventLoopThreadPool(int loop_count) : loop_count_(loop_count) {}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::Start() {
    for (int i = 0; i < loop_count_; i++) {
        auto thread = std::make_unique<EventLoopThread>();
        loops_.emplace_back(thread->StartLoop());
        loop_threads_.emplace_back(std::move(thread));
    }
}

EventLoop* EventLoopThreadPool::GetNextLoop() {
    if (loops_.empty()) {
        return nullptr;
    }
    int current_index = next_.fetch_add(1);
    return loops_[current_index % loops_.size()];
}

}