#include "executor.h"

namespace ctx {

void Executor::ExecutorTimer::Start() {
  if (is_closed_.exchange(false)) {
    threadpool_.Start();
    threadpool_.RunTask(&Executor::ExecutorTimer::Run_, this);
  }
}

void Executor::ExecutorTimer::Stop() {
  if (is_closed_.exchange(true)) {
    return;
  }
  tasks_cv_.notify_all();
  threadpool_.Stop();
  tasks_ = std::priority_queue<TaskInfo, std::vector<TaskInfo>,
                               std::greater<TaskInfo>>();
}

// 循环取出任务队列的顶部元素，执行任务或等待
void Executor::ExecutorTimer::Run_() {
  while (true) {
    TaskInfo task_info;
    {
      std::unique_lock<std::mutex> lock(tasks_mutex_);
      tasks_cv_.wait(lock,
                     [this]() { return !tasks_.empty() || is_closed_.load(); });
      if (is_closed_.load()) {
        break;
      }
      TimePointType next_exec_time = tasks_.top().exec_time_point;

      // 判断取出的任务是否到了执行的时间点
      if (next_exec_time > std::chrono::high_resolution_clock::now()) {
        // 没到，等待到时间点 或者 被 唤醒（加入新任务）
        tasks_cv_.wait_until(lock, next_exec_time);
        // 因为可能加入新任务，重新循环来做判断
        continue;
      } else {
        task_info = std::move(tasks_.top());
        tasks_.pop();
      }
    }
    task_info.task();
  }
}

void Executor::ExecutorTimer::PostDelayedTask(
    Task task, const std::chrono::microseconds& delay) {
  PostTask_(std::move(task), delay, 0);
}

RepeatedTaskId Executor::ExecutorTimer::PostRepeatedTask(
    Task task, const std::chrono::microseconds& delay, uint64_t repeat_counts) {
  RepeatedTaskId id = GetNextRepeatedId();
  {
    std::lock_guard<std::mutex> lock(repeated_task_mutex_);
    repeated_task_id_set_.insert(id);
  }

  // 首次提交
  Task first_task = std::bind(&Executor::ExecutorTimer::PostRepeatedTask_, this, std::move(task), delay, id, repeat_counts);
  PostTask_(std::move(first_task), delay, id);

  return id;
}

// 添加一个taskinfo到任务队列
void Executor::ExecutorTimer::PostTask_(Task task,
                                        const std::chrono::microseconds& delay,
                                        RepeatedTaskId id) {
  std::chrono::time_point<std::chrono::high_resolution_clock> exec_time_point =
      std::chrono::high_resolution_clock::now() + delay;
  {
    std::lock_guard<std::mutex> lock(tasks_mutex_);
    tasks_.push({std::move(task), id, exec_time_point});
  }
  tasks_cv_.notify_one();
}

// 递归 执行task并提交自己到任务队列
void Executor::ExecutorTimer::PostRepeatedTask_(Task task, const std::chrono::microseconds& delay, RepeatedTaskId id, uint64_t repeat_counts) {
  {
    std::lock_guard<std::mutex> lock(repeated_task_mutex_);
    if (repeated_task_id_set_.find(id) == repeated_task_id_set_.end() ||
        repeat_counts == 0) {
      return;
    }
  }
  task();
  repeat_counts--;
  if (repeat_counts > 0) {
    Task next_task = std::bind(&Executor::ExecutorTimer::PostRepeatedTask_, this, std::move(task), delay, id, repeat_counts);
    PostTask_(std::move(next_task), delay, id);
  }
}

void Executor::ExecutorTimer::CancelRepeatedTask(RepeatedTaskId id) {
  std::lock_guard<std::mutex> lock(repeated_task_mutex_);
  repeated_task_id_set_.erase(id);
}

TaskRunnerTag Executor::ExecutorContext::AddStrandRunner(TaskRunnerTag tag) {
  std::lock_guard<std::mutex> lock(task_runner_mutex_);
  // 用户指定tag，若指定的不可用就加一，循环
  while (task_runner_map_.find(tag) != task_runner_map_.end()) {
    tag = GetNextRunnerTag();
  }
  task_runner_map_.emplace(std::piecewise_construct, std::forward_as_tuple(tag),
                           std::forward_as_tuple(1));
  return tag;
}

TaskRunnerTag Executor::ExecutorContext::AddParallelRunner(TaskRunnerTag tag, int thread_count) {
  std::lock_guard<std::mutex> lock(task_runner_mutex_);
  while (task_runner_map_.find(tag) != task_runner_map_.end()) {
    tag = GetNextRunnerTag();
  }
  task_runner_map_.emplace(std::piecewise_construct, std::forward_as_tuple(tag),
                           std::forward_as_tuple(thread_count));
  return tag;
}

Executor::ExecutorContext::TaskRunner* Executor::ExecutorContext::GetTaskRunner(
    TaskRunnerTag tag) {
  std::lock_guard<std::mutex> lock(task_runner_mutex_);
  auto it = task_runner_map_.find(tag);
  if (it == task_runner_map_.end()) {
    return nullptr;
  }
  return &it->second;
}

void Executor::PostTask(TaskRunnerTag tag, Task task) {
  Executor::ExecutorContext::TaskRunner* runner_ptr =
      executor_context_.GetTaskRunner(tag);
  runner_ptr->RunTask(std::move(task));
}

TaskRunnerTag Executor::AddStrandRunner(TaskRunnerTag tag) {
  return executor_context_.AddStrandRunner(tag);
}

TaskRunnerTag Executor::AddParallelRunner(TaskRunnerTag tag, int thread_count) {
  return executor_context_.AddParallelRunner(tag, thread_count);
}



}  // namespace ctx
