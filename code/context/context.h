#pragma once
#include <memory>

#include "executor.h"

namespace ctx {

class Context {
 public:
  ~Context();

  Executor* GetExecutor();

  static Context* GetInstance() {
    static Context instance;
    return &instance;
  }

  TaskRunnerTag NewStrandRunner(TaskRunnerTag tag);

  TaskRunnerTag NewParallelRunner(TaskRunnerTag tag, int thread_count);

  Context(const Context& other) = delete;
  Context& operator=(const Context& other) = delete;

 private:
  Context();

 private:
  class ExecutorManager;
  std::unique_ptr<ExecutorManager> executor_manager_;
};

}  // namespace ctx

#define CONTEXT ctx::Context::GetInstance()

#define EXECUTOR CONTEXT->GetExecutor()

#define NEW_STRAND_RUNNER(tag) CONTEXT->NewStrandRunner(tag)

#define NEW_PARALLEL_RUNNER(tag, thread_count) CONTEXT->NewParallelRunner(tag, thread_count);

#define POST_TASK(tag, task) EXECUTOR->PostTask(tag, task)

#define WAIT_TASK_IDLE(tag) EXECUTOR->PostRetTask(tag, []() {}).wait()

#define POST_REPEATED_TASK(tag, task, delay, repeat_counts) \
  EXECUTOR->PostRepeatedTask(tag, task, delay, repeat_counts)

#define CANCEL_REPEATED_TASK(id) EXECUTOR->CancelRepeatedTask(id)
