#pragma once
#include <Scheduler.hpp>
#include <asio/steady_timer.hpp>

template <typename Rep, typename Period>
class SleepAwaiter {
public:
  bool suspend_ready() { return false; };
  void await_suspend(std::coroutine_handle<> outer);
  void await_resume();

private:
  std::chrono::duration<Rep, Period> interval_;
};

template <typename Rep, typename Period>
void SleepAwaiter<Rep, Period>::await_suspend(std::coroutine_handle<> outer) {
  Scheduler* shed = Scheduler::GetCurrentScheduler();
  if (!shed) {
    throw std::runtime_error("No current scheduler");
  }
  asio::steady_timer timer(shed->io_context_, interval_);
  timer.async_wait([shed, outer = std::move(outer)] () {
    shed->Schedule(outer);
  });
}

template <typename Rep, typename Period>
SleepAwaiter<Rep, Period> SleepFor(std::chrono::duration<Rep, Period> interval) { return {interval}; }
