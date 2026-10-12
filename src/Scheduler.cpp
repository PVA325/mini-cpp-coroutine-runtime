#include "Scheduler.hpp"
#include "Task.hpp"

void Scheduler::Spawn(Task<void>&& task) {
  std::coroutine_handle<> coro = std::exchange(task.handle_, nullptr);
  handles_queue_.emplace_back(coro);
}

void Scheduler::Run() {
  scheduler_ptr_ = this;
  while (!handles_queue_.empty() && !stopped_) {
    std::coroutine_handle<> handle = std::move(handles_queue_.front());
    handles_queue_.pop_front();
    handle.resume();

    DestroyHandles();
  }
  DestroyHandles();
  scheduler_ptr_ = nullptr;
}

void Scheduler::Stop() {
  stopped_ = true;
}

void Scheduler::Schedule(std::coroutine_handle<> handle) {
  handles_queue_.emplace_back(handle);
}

void Scheduler::ScheduleDestroing(std::coroutine_handle<> handle) {
  handles_to_destroy_.push_back(handle);
}

inline void Scheduler::DestroyHandles() {
  while (!handles_to_destroy_.empty()) {
    std::coroutine_handle<> handle = handles_to_destroy_.front();
    handles_to_destroy_.pop_front();
    handle.destroy();
  }
}



