#include "Yield.hpp"

#include "Scheduler.hpp"

#include <stdexcept>

bool YieldAwaiter::await_ready() const { return false; }

void YieldAwaiter::await_suspend(std::coroutine_handle<> outer) {
  Scheduler* shed = Scheduler::GetCurrentScheduler();
  if (!shed) {
    throw std::runtime_error("No current scheduler");
  }
  shed->Schedule(outer);
}

void YieldAwaiter::await_resume() {}

YieldAwaiter Yield() { return {}; }
