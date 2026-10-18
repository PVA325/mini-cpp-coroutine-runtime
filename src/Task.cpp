#include "Task.hpp"

Task<void> TaskPromise<void>::get_return_object() {
  return Task<void>(std::coroutine_handle<TaskPromise<void>>::from_promise(*this));
}

void TaskPromise<void>::return_void() {}

void TaskPromise<void>::GetReturnedValue() const {}

void TaskPromise<void>::unhandled_exception() {
  exception_ = std::current_exception();
}

std::exception_ptr TaskPromise<void>::GetException() const { return exception_; }

void Task<void>::TaskAwaiter::await_resume() {
  auto exception_ptr = this->handle_.promise().GetException();
  if (exception_ptr) {
    throw exception_ptr;
  }
}
