#pragma once
#include <coroutine>
#include <expected>
#include <format>
#include <stdexcept>
#include <utility>


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
template <typename T>
struct TaskAwaiterBase;

/////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////              CLASS DECLARATIONS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////            TaskPromiseBase<T> CLASS            /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class TaskPromiseBase {
public:
  struct final_awaiter;

  std::suspend_always initial_suspend() noexcept { return {}; }

  final_awaiter final_suspend() noexcept { return {}; }

  ~TaskPromiseBase() = default;

protected:
  std::coroutine_handle<> continuation_{nullptr};

  template <typename U>
  friend class Task;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////            TaskPromise<T> CLASS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class TaskPromise : public TaskPromiseBase<T> {
public:
  Task<T> get_return_object();

  void return_value(const T& val) { res_ = val; }

  void unhandled_exception() { res_ = std::unexpected(std::current_exception()); }

  T GetReturnedValue() const;

  ~TaskPromise() = default;

protected:
  std::expected<T, std::exception_ptr> res_;

  template <typename U>
  friend class TaskAwaiterBase;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////         TaskPromise<void> SPECIALIZATION        /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <>
class TaskPromise<void> : public TaskPromiseBase<void> {
public:
  Task<void> get_return_object();
  void return_void() {}

  void GetReturnedValue() const {}
  void unhandled_exception() { exception_ = std::current_exception(); }
  std::exception_ptr GetException() const { return exception_; }

  ~TaskPromise() = default;
private:
  std::exception_ptr exception_;

  template <typename U>
  friend class TaskAwaiterBase;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////               Task<T> CLASS                   /////////////////////////////
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
  TaskAwaiter operator co_await() && noexcept;

  ~Task();

private:
  Task(std::coroutine_handle<promise_type> handle): handle_(handle) {}

private:
  std::coroutine_handle<promise_type> handle_;

  friend class TaskPromise<T>;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////         TaskAwaiterBase<T> CLASS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct TaskAwaiterBase {
  TaskAwaiterBase(std::coroutine_handle<TaskPromise<T>> handle): handle_(handle) {}

  bool await_ready() const noexcept { return false; }

  auto await_suspend(std::coroutine_handle<> outer_coro) noexcept -> std::coroutine_handle<>;

  ~TaskAwaiterBase();
protected:
  std::coroutine_handle<TaskPromise<T>> handle_;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////       Task<T>::TaskAwaiter CLASS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct Task<T>::TaskAwaiter : public TaskAwaiterBase<T> {
  TaskAwaiter(std::coroutine_handle<TaskPromise<T>> handle): TaskAwaiterBase<T>(handle) {}

  T await_resume();

  ~TaskAwaiter() = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////     Task<void>::TaskAwaiter SPECIALIZATION    /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <>
struct Task<void>::TaskAwaiter : public TaskAwaiterBase<void> {
  TaskAwaiter(std::coroutine_handle<TaskPromise<void>> handle): TaskAwaiterBase<void>(handle) {}

  void await_resume();

  ~TaskAwaiter() = default;
};

/////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////           METHOD IMPLEMENTATIONS              //////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////        TaskPromiseBase<T> METHODS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
struct TaskPromiseBase<T>::final_awaiter {
  bool await_ready() const noexcept { return false; }

  auto await_suspend(std::coroutine_handle<TaskPromise<T>> last_coro) noexcept
    -> std::coroutine_handle<>;

  void await_resume() noexcept {}
};

template <typename T>
auto TaskPromiseBase<T>::final_awaiter::await_suspend(
  std::coroutine_handle<TaskPromise<T>> last_coro) noexcept -> std::coroutine_handle<> {
  if (last_coro.promise().continuation_) {
    return last_coro.promise().continuation_;
  }
  return std::noop_coroutine();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////        TaskPromise<T> METHODS                 /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T TaskPromise<T>::GetReturnedValue() const {
  if (!res_) {
    std::rethrow_exception(res_.error());
  }
  return res_.value();
}

template <typename T>
Task<T> TaskPromise<T>::get_return_object() {
  return Task<T>(std::coroutine_handle<TaskPromise<T>>::from_promise(*this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////      TaskPromise<void> METHODS                /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

Task<void> TaskPromise<void>::get_return_object() {
  return Task<void>(std::coroutine_handle<TaskPromise<void>>::from_promise(*this));
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////           Task<T> METHODS                     /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

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
typename Task<T>::TaskAwaiter Task<T>::operator co_await() && noexcept {
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
/////////////////////////      TaskAwaiterBase<T> METHODS               /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
TaskAwaiterBase<T>::~TaskAwaiterBase() {
  if (handle_) {
    handle_.destroy();
  }
}

template <typename T>
auto TaskAwaiterBase<T>::await_suspend(std::coroutine_handle<> outer_coro) noexcept
  -> std::coroutine_handle<> {
  handle_.promise().continuation_ = outer_coro;
  return handle_;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////   Task<T>::TaskAwaiter METHODS                /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
T Task<T>::TaskAwaiter::await_resume() {
  return this->handle_.promise().GetReturnedValue();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////  Task<void>::TaskAwaiter METHODS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

void Task<void>::TaskAwaiter::await_resume() {
  auto exception_ptr = this->handle_.promise().GetException();
  if (exception_ptr) {
    throw exception_ptr;
  }
}
