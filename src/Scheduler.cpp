#include "Scheduler.hpp"
#include "Task.hpp"

void Scheduler::Spawn(Task<void>&& task) {
  if (stopped_) {
    return;
  }
  std::coroutine_handle<> coro = std::exchange(task.handle_, nullptr);
  handles_queue_.emplace_back(coro);
}
void Scheduler::Run() {
  scheduler_ptr_ = this;
  while (!handles_queue_.empty() || io_context_.poll()) {
    std::coroutine_handle<> handle = std::move(handles_queue_.front());
    handles_queue_.pop_front();
    handle.resume();

    if (NeedCleanPoll()) {
      io_context_.poll();
    }
    if (NeedDestroyReadyHandlers()) {
      DestroyReadyHandles();
    }
  }
  assert(io_context_.poll() == 0); // TODO(MY): write own debbuging macroses
  DestroyReadyHandles();
  scheduler_ptr_ = nullptr;
}

// TODO(add to docs): need to call Run after Stop to complete current coroutines
// otherwise there no garanty for memory cleanup(coro frames can be not destroyed)
void Scheduler::Stop() {
  stopped_ = true;
}

void Scheduler::Schedule(std::coroutine_handle<> handle) {
  handles_queue_.emplace_back(handle);
}

void Scheduler::ScheduleDestroing(std::coroutine_handle<> handle) {
  handles_to_destroy_.push_back(handle);
}

inline void Scheduler::DestroyReadyHandles() {
  while (!handles_to_destroy_.empty()) {
    std::coroutine_handle<> handle = handles_to_destroy_.front();
    handles_to_destroy_.pop_front();
    handle.destroy();
  }
}

bool Scheduler::NeedCleanPoll() {
  return true;
}

bool Scheduler::NeedDestroyReadyHandlers() {
  return true;
}



