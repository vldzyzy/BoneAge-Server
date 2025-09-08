#include "context.h"

#include "executor.h"


namespace ctx {

class Context::ExecutorManager {
 public:
  ExecutorManager() = default;
  ~ExecutorManager() = default;

  Executor* GetExecutor() { return &executor_; }

  TaskRunnerTag NewStrandRunner(TaskRunnerTag tag) {
    return executor_.AddStrandRunner(tag);
  }

  TaskRunnerTag NewParallelRunner(TaskRunnerTag tag, int thread_count) {
    return executor_.AddParallelRunner(tag, thread_count);
  }

 private:
  Executor executor_;
};

Context::Context() : executor_manager_(std::make_unique<ExecutorManager>()) {}

Context::~Context() = default;

Executor* Context::GetExecutor() { return executor_manager_->GetExecutor(); }

TaskRunnerTag Context::NewStrandRunner(TaskRunnerTag tag) {
  return executor_manager_->NewStrandRunner(tag);
}

TaskRunnerTag Context::NewParallelRunner(TaskRunnerTag tag, int thread_count) {
  return executor_manager_->NewParallelRunner(tag, thread_count);
}

}  // namespace ctx