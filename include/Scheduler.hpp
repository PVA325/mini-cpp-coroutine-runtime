#pragma once
#include <deque>
#include <common.hpp>
#include <coroutine>
#include <asio/io_context.hpp>

class Scheduler {
public:
  // Add new task, and scheduler owns it(destroys after completion)
  void Spawn(Task<void>&& task);

  void Run();
  void Stop();

  // if called from coro that is executed in sheduler -> returns its ptr
  // otherwise if it is called outside, returns nullptr
  static Scheduler *GetCurrentScheduler() { return  scheduler_ptr_; }

  void ScheduleDestroing(std::coroutine_handle<> handle);
private:
  // Add new task, and scheduler DON'T owns it(destroys after completion)
  void Schedule(std::coroutine_handle<> handle);
  void DestroyReadyHandles();
  bool NeedCleanPoll();
  bool NeedDestroyReadyHandlers();

  std::deque<std::coroutine_handle<>> handles_queue_;
  std::deque<std::coroutine_handle<>> handles_to_destroy_;
  bool stopped_{false};
  asio::io_context io_context_;
  inline static Scheduler* scheduler_ptr_{nullptr};

  friend class YieldAwaiter;
  template <typename Rep, typename Period>
  friend class SleepAwaiter;
};
