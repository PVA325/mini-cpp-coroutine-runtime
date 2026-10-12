#pragma once
#include "Scheduler.hpp"

class YieldAwaiter {
public:
  bool await_ready() const { return false; }
  void await_suspend(std::coroutine_handle<> outer);
  void await_resume() {}
};

inline void YieldAwaiter::await_suspend(std::coroutine_handle<> outer) {
  Scheduler* shed = Scheduler::GetCurrentScheduler();
  if (!shed) {
    throw std::runtime_error("No current scheduler");
  }
  shed->Schedule(outer);
}

inline YieldAwaiter Yield() { return {}; }
