#include "thread_pool.h"

namespace ctx {

void ThreadPool::Start() {
  if (is_closed_.exchange(false)) {
    uint32_t thread_counts = thread_counts_.load();
    for (uint32_t i = 0; i < thread_counts; i++) {
      AddThread_();
    }
  }
}

void ThreadPool::Stop() {
  if (is_closed_.exchange(true)) {
    return;
  }
  tasks_cv_.notify_all();
  workers_.clear();
}

void ThreadPool::AddThread_() {
  // 线程所执行的函数
  auto func = [this]() {
    while (true) {
      Task task;
      {
        std::unique_lock<std::mutex> lock(tasks_mutex_);
        tasks_cv_.wait(
            lock, [this]() { return !tasks_.empty() || is_closed_.load(); });
        if (is_closed_.load()) {
          break;
        }
        task = std::move(tasks_.front());
        tasks_.pop();
      }
      task();
    }
  };

  ThreadInfoPtr thread_ptr = std::make_shared<ThreadInfo>();
  thread_ptr->ptr = std::make_shared<std::thread>(func);
  workers_.emplace_back(std::move(thread_ptr));
}
}  // namespace ctx