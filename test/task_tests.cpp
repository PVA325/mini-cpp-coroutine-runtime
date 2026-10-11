#include <gtest/gtest.h>

#include <Task.hpp>

#include <cstdint>

namespace {

Task<int> SimpleTask() {
  co_return 42;
}

Task<int> AddOne(int value) {
  co_return value + 1;
}

Task<int> SequentialTask() {
  int first = co_await AddOne(1);
  int second = co_await AddOne(first);
  co_return second + 1;
}

Task<std::int64_t> DeepRecursiveSum(int n) {
  if (n == 0) {
    co_return 0;
  }

  std::int64_t prev = co_await DeepRecursiveSum(n - 1);
  co_return prev + n;
}

}  // namespace

TEST(TaskTest, SimpleResumeAndValue) {
  Task<int> task = SimpleTask();

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), 42);
}

TEST(TaskTest, SequentialAwaitOfTemporaryTasks) {
  Task<int> task = SequentialTask();

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), 4);
}

TEST(TaskTest, DeepRecursiveAwaitDoesNotOverflowStack) {
  constexpr int kDepth = 20000;
  const std::int64_t expected = static_cast<std::int64_t>(kDepth) * (kDepth + 1) / 2;

  Task<std::int64_t> task = DeepRecursiveSum(kDepth);

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), expected);
}

