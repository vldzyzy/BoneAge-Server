#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <iostream>
#include <map>
#include <mutex>
#include <queue>
#include <unordered_set>

#include "thread_pool.h"

namespace ctx {

using Task = std::function<void()>;
using RepeatedTaskId = uint64_t;
using TaskRunnerTag = uint64_t;

class Executor {
 private:
  // 管理定时事件
  class ExecutorTimer {
   private:
    using TimePointType =
        std::chrono::time_point<std::chrono::high_resolution_clock>;

    struct TaskInfo {
      Task task;
      RepeatedTaskId repeated_id;
      TimePointType exec_time_point;

      bool operator>(const TaskInfo& rhs) const {
        return exec_time_point > rhs.exec_time_point;
      }
    };

   public:
    ExecutorTimer() : threadpool_(1) {}

    ~ExecutorTimer() { Stop(); }

    void Start();

    void Stop();

    void PostDelayedTask(Task task, const std::chrono::microseconds& delay);

    RepeatedTaskId PostRepeatedTask(Task task,
                                    const std::chrono::microseconds& delay,
                                    uint64_t repeat_counts);

    void CancelRepeatedTask(RepeatedTaskId id);

   private:
    void Run_();

    void PostTask_(Task task, const std::chrono::microseconds& delay,
                   RepeatedTaskId id);

    void PostRepeatedTask_(Task task, const std::chrono::microseconds& delay, RepeatedTaskId id, uint64_t repeat_counts);
    RepeatedTaskId GetNextRepeatedId() { return repeated_task_id_++; }

   private:
    std::priority_queue<TaskInfo, std::vector<TaskInfo>, std::greater<TaskInfo>>
        tasks_;
    std::mutex tasks_mutex_;
    std::condition_variable tasks_cv_;

    std::atomic<bool> is_closed_{true};

    ThreadPool threadpool_;
    std::atomic<RepeatedTaskId> repeated_task_id_{1};
    std::unordered_set<RepeatedTaskId> repeated_task_id_set_;
    std::mutex repeated_task_mutex_;
  };

  // 管理所有strand
  class ExecutorContext {
   public:
    ExecutorContext() = default;
    ~ExecutorContext() = default;

    ExecutorContext(const ExecutorContext& other) = delete;
    ExecutorContext& operator=(const ExecutorContext& other) = delete;

    TaskRunnerTag AddStrandRunner(TaskRunnerTag tag);

    TaskRunnerTag AddParallelRunner(TaskRunnerTag tag, int thread_count);

   private:
    using TaskRunner = ThreadPool;
    using TaskRunnerPtr = std::unique_ptr<TaskRunner>;
    friend class Executor;

   private:
    TaskRunner* GetTaskRunner(TaskRunnerTag tag);

    TaskRunnerTag GetNextRunnerTag() { return task_runner_tag_++; }

   private:
    TaskRunnerTag task_runner_tag_ = 1; // tag从1开始
    std::map<TaskRunnerTag, TaskRunner> task_runner_map_;
    std::mutex task_runner_mutex_;
  };

 public:
  Executor() = default;
  ~Executor() = default;

  Executor(const Executor& other) = delete;
  Executor& operator=(const Executor& other) = delete;

  TaskRunnerTag AddStrandRunner(TaskRunnerTag tag);

  TaskRunnerTag AddParallelRunner(TaskRunnerTag tag, int thread_count);

  void PostTask(TaskRunnerTag tag, Task task);

  template <typename R, typename P>
  void PostDelayedTask(TaskRunnerTag tag, Task task,
                       const std::chrono::duration<R, P>& delay) {
    Task func = std::bind(&Executor::PostTask, this, tag, std::move(task));

    executor_timer_.Start();
    executor_timer_.PostDelayedTask(
        std::move(func),
        std::chrono::duration_cast<std::chrono::microseconds>(delay));
  }

  template <typename R, typename P>
  RepeatedTaskId PostRepeatedTask(TaskRunnerTag tag, Task task,
                                  const std::chrono::duration<R, P>& delay,
                                  uint64_t repeat_counts) {
    Task func = std::bind(&Executor::PostTask, this, tag, std::move(task));

    executor_timer_.Start();
    return executor_timer_.PostRepeatedTask(
        std::move(func),
        std::chrono::duration_cast<std::chrono::microseconds>(delay),
        repeat_counts);
  }

  void CancelRepeatedTask(RepeatedTaskId id) {
    executor_timer_.CancelRepeatedTask(id);
  }

  template <typename F, typename... Args>
  auto PostRetTask(TaskRunnerTag tag, F&& f, Args&&... args)
      -> std::future<std::invoke_result_t<F, Args...>> {
    using return_type = std::invoke_result_t<F, Args...>;
    ExecutorContext::TaskRunner* runner = executor_context_.GetTaskRunner(tag);
    return runner->RunRetTask(std::forward<F>(f), std::forward<Args>(args)...);
  }

 private:
  ExecutorContext executor_context_;
  ExecutorTimer executor_timer_;
};

}  // namespace ctx