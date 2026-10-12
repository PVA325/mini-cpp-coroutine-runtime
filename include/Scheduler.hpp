#pragma once
#include <atomic>
#include <deque>
#include <Task.hpp>

class Scheduler {
public:
  // Add new task, and scheduler owns it(destroys after completion)
  void Spawn(Task<void>&& task);
  void Run();
  void Stop();

  // if called from coro that is executed in sheduler -> returns its ptr
  // otherwise if it is called outside, returns nullptr
  static Scheduler *GetCurrentScheduler() { return  scheduler_ptr_; }
private:
  // Add new task, and scheduler DON'T owns it(destroys after completion)
  void Schedule(std::coroutine_handle<> handle);

  struct Node {
    std::coroutine_handle<> task;
    bool owned{false};
  };

  std::deque<Node> queue_;
  bool stopped_{false};
  inline static Scheduler* scheduler_ptr_{nullptr};
};

inline void Scheduler::Spawn(Task<void>&& task) {
  std::coroutine_handle<> coro = std::exchange(task.handle_, nullptr);
  queue_.emplace_back(coro, true);
}

void Scheduler::Run() {
  scheduler_ptr_ = this;
  while (!queue_.empty() && !stopped_) {
    Node node = queue_.front();
    queue_.pop_front();
    node.task.resume();
    if (!node.task.done()) {
      queue_.push_back(std::move(node));
      continue;
    }
    if (node.owned) {
      node.task.destroy();
    }
  }
  scheduler_ptr_ = nullptr;
}

void Scheduler::Stop() {
  stopped_ = true;
}

inline void Scheduler::Schedule(std::coroutine_handle<> handle) {
  queue_.emplace_back(handle, false);
}


