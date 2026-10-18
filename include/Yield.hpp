#pragma once
#include <coroutine>

class YieldAwaiter {
public:
  bool await_ready() const;
  void await_suspend(std::coroutine_handle<> outer);
  void await_resume();
};

YieldAwaiter Yield();
