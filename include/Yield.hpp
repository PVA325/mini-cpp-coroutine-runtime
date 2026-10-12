#pragma once
#include "Scheduler.hpp"

class YieldAwaiter {
public:
  bool await_ready() const { return false; }
  void await_suspend(std::coroutine_handle<> outer);
  void await_resume() {}
};

inline void YieldAwaiter::await_suspend(std::coroutine_handle<> outer) {
  Scheduler::GetCurrentScheduler()->Schedule(outer);
}

inline YieldAwaiter Yield() { return {}; }
