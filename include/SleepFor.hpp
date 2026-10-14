#pragma once
#include <Scheduler.hpp>
#include <asio/steady_timer.hpp>

template <typename Rep, typename Period>
class SleepAwaiter {
public:
  SleepAwaiter(std::chrono::duration<Rep, Period> interval) : interval_(std::move(interval)) {}
  bool await_ready() { return false; };
  void await_suspend(std::coroutine_handle<> outer);
  void await_resume();

private:
  std::chrono::duration<Rep, Period> interval_;
  asio::error_code err_;
  std::optional<asio::steady_timer> timer_;
};

template <typename Rep, typename Period>
void SleepAwaiter<Rep, Period>::await_suspend(std::coroutine_handle<> outer) {
  Scheduler* shed = Scheduler::GetCurrentScheduler();
  if (!shed) {
    throw std::runtime_error("No current scheduler");
  }
  timer_.emplace(shed->io_context_, interval_);
  auto cb = [&err = this->err_, shed, outer = std::move(outer)] (const asio::error_code& error) {
    if (error) {
      err = error;
    }
    shed->Schedule(outer);
    --shed->context_tasks_;
  };
  ++shed->context_tasks_;
  timer_->async_wait(cb);
}

template <typename Rep, typename Period>
void SleepAwaiter<Rep, Period>::await_resume() {
  if (err_) {
    throw asio::system_error(err_);
  }
}

template <typename Rep, typename Period>
SleepAwaiter<Rep, Period> SleepFor(std::chrono::duration<Rep, Period> interval) { return {std::move(interval)}; }
