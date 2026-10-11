#pragma once
#include <coroutine>
#include <expected>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>

// type void will be invalid with my Task, we should use std::monostate instead

#define ASSERT_HANDLE_VALID(handle, context_msg) \
    do { \
        if (!(handle)) { \
            throw std::runtime_error(std::format("[{}] Error: Task has Empty handle", context_msg)); \
        } \
    } while (0)

template <typename T>
class Task;
template <typename T>
class TaskPromise;

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                  TaskPromise          /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////


template <typename T>
class TaskPromise {
public:
  struct final_awaiter;
  // creates Task from this promise
  // Task handle will be pointed to the beginning of coro frame
  Task<T> get_return_object();

  // always initial suspend for Spawn, because we want full control, when to start a Task
  std::suspend_always initial_suspend() noexcept { return {}; }
  // Same for suspend
  final_awaiter final_suspend() noexcept {
    return {};
  }

  // if coroutine ends and return value, we just write in Promise
  void return_value(const T& val) { res_ = val; }

  void unhandled_exception() { res_ = std::unexpected(std::current_exception()); }

  T GetReturnedValue() const;

  ~TaskPromise() = default;
private:
  std::expected<T, std::exception_ptr> res_;
  std::coroutine_handle<> continuation_{nullptr};
};

template <typename T>
Task<T> TaskPromise<T>::get_return_object() {
  return Task<T>(std::coroutine_handle<TaskPromise<T>>::from_promise(*this));
}

template <typename T>
T TaskPromise<T>::GetReturnedValue() const {
  if (!res_) {
    std::rethrow_exception(res_.error());
  }
  return res_.value();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                  TASK CLASS           /////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class Task {
public:
  using promise_type = TaskPromise<T>;

  Task(const Task&) = delete;
  Task& operator=(const Task&) = delete;
  Task(Task&& other);

  Task& operator=(Task&& other);

  void Resume();
  bool IsDone() const;
  T GetValue() const { return handle_.promise().GetReturnedValue(); }

  const promise_type& Promise() const { return handle_.promise(); }
  auto Handler() const -> std::coroutine_handle<promise_type> { return handle_; }

  struct TaskAwaiter;
  TaskAwaiter operator co_await() const && noexcept;

  ~Task();

private:
  Task(std::coroutine_handle<promise_type> handle): handle_(handle) {}

private:
  std::coroutine_handle<promise_type> handle_;

  friend class TaskPromise<T>;
};

template <typename T>
Task<T>::Task(Task&& other): handle_(std::move(other.handle_)) {
  other.handle_ = nullptr;
}

template <typename T>
Task<T>& Task<T>::operator=(Task&& other) {
  if (this != &other) {
    if (handle_) {
      handle_.destroy();
    }
    handle_ = std::move(other.handle_);
    other.handle_ = nullptr;
  }
  return *this;
}

template <typename T>
void Task<T>::Resume() {
  ASSERT_HANDLE_VALID(handle_, "Task::Resume");
  handle_.resume();
}

template <typename T>
bool Task<T>::IsDone() const {
  ASSERT_HANDLE_VALID(handle_, "Task::IsDone");
  return handle_.done();
}

template <typename T>
typename Task<T>::TaskAwaiter Task<T>::operator co_await() const && noexcept {
  auto h = std::exchange(handle_, nullptr);
  return TaskAwaiter(h);
}

template <typename T>
Task<T>::~Task() {
  if (handle_) {
    handle_.destroy();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////                  TASK Awaiter           ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct Task<T>::TaskAwaiter {
  TaskAwaiter(std::coroutine_handle<TaskPromise<T>> handle): handle_(handle) {}

  bool await_ready() const noexcept { return false; }

  auto await_suspend(std::coroutine_handle<> outer_coro) noexcept -> std::coroutine_handle<>;

  T await_resume();

  ~TaskAwaiter();
private:
  std::coroutine_handle<TaskPromise<T>> handle_;
};

template <typename T>
auto Task<T>::TaskAwaiter::await_suspend(std::coroutine_handle<> outer_coro) noexcept
  -> std::coroutine_handle<> {
  handle_.promise().continuation_ = outer_coro;
  return handle_;
}

template <typename T>
T Task<T>::TaskAwaiter::await_resume() {
  const auto& res = handle_.promise().res_;
  if (!res) {
    throw res.error();
  }
  return res.value();
}

template <typename T>
Task<T>::TaskAwaiter::~TaskAwaiter() {
  if (handle_) {
    handle_.destroy();
  }
}


/////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////                  TASK Final Awaiter           ///////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct TaskPromise<T>::final_awaiter {
  bool await_ready() const noexcept { return false; }

  auto await_suspend(std::coroutine_handle<TaskPromise<T>> last_coro) noexcept
    -> std::coroutine_handle<>;

  void await_resume() noexcept {}
};

template <typename T>
auto TaskPromise<T>::final_awaiter::await_suspend(
  std::coroutine_handle<TaskPromise<T>> last_coro) noexcept -> std::coroutine_handle<> {
  if (last_coro.promise().continuation_) {
    return last_coro.promise().continuation_;
  }
  return std::noop_coroutine();
}
