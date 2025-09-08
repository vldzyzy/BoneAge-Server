#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace ctx {

class ThreadPool {
 public:
  explicit ThreadPool(uint32_t thread_counts)
      : thread_counts_(thread_counts), is_closed_(true) {
    Start();
  }

  ~ThreadPool() { Stop(); }

  ThreadPool(const ThreadPool&) = delete;
  ThreadPool& operator=(const ThreadPool&) = delete;

  void Start();

  void Stop();

  template <typename F, typename... Args>
  void RunTask(F&& f, Args&&... args) {
    if (is_closed_.load()) {
      return;
    }
    Task task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      tasks_.emplace(std::move(task));
    }
    tasks_cv_.notify_one();
  }

  template <typename F, typename... Args>
  auto RunRetTask(F&& f, Args&&... args)
      -> std::future<std::invoke_result_t<F, Args...>> {
    if (is_closed_.load()) {
      return std::future<std::invoke_result_t<F, Args...>>();
    }
    using return_type = std::invoke_result_t<F, Args...>;
    auto task_pkg = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    auto result = task_pkg->get_future();
    Task task = [task_pkg]() { (*task_pkg)(); };
    {
      std::lock_guard<std::mutex> lock(tasks_mutex_);
      tasks_.emplace(std::move(task));
    }
    tasks_cv_.notify_one();
    return result;
  }

 private:
  void AddThread_();

 private:
  using ThreadPtr = std::shared_ptr<std::thread>;
  struct ThreadInfo {
    ThreadInfo() = default;
    ~ThreadInfo() {
      if (ptr && ptr->joinable()) {
        ptr->join();
      }
    }

    ThreadPtr ptr;
  };

  using ThreadInfoPtr = std::shared_ptr<ThreadInfo>;
  using Task = std::function<void()>;

 private:
  std::vector<ThreadInfoPtr> workers_;
  std::queue<Task> tasks_;

  std::mutex tasks_mutex_;
  std::condition_variable tasks_cv_;

  std::atomic<uint32_t> thread_counts_;
  std::atomic<bool> is_closed_;
};
}  // namespace ctx