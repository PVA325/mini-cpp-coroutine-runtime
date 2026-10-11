#include <iostream>
#include <Task.hpp>

Task<int> f() {
  co_return 42;
}

int main() {
  Task<int> res = f();
  std::cout << res.IsDone() << "\n";
  res.Resume();
  std::cout << res.IsDone() << "\n";
  std::cout << res.GetValue();
}