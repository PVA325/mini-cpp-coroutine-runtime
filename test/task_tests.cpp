#include <gtest/gtest.h>

#include <Task.hpp>
#include <Scheduler.hpp>
#include <Yield.hpp>
#include <SleepFor.hpp>

#include <cstdint>
#include <vector>

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

Task<void> VoidLeaf() {
  co_return;
}

Task<void> VoidSequence() {
  co_await VoidLeaf();
  co_await VoidLeaf();
  co_return;
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
  // This test works with AddressSanitizer.
  // Even with ASAN, we use moderate depth (1000) because ASAN with
  // -fno-omit-frame-pointer prevents aggressive tail call optimization.
  constexpr int kDepth = 1000;
  const std::int64_t expected = static_cast<std::int64_t>(kDepth) * (kDepth + 1) / 2;

  Task<std::int64_t> task = DeepRecursiveSum(kDepth);

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), expected);
}

// This test requires -O2/-O3 optimization and NO AddressSanitizer to run successfully.
// It tests that symmetric transfer truly avoids stack overflow with extreme recursion depth.
// Only runs in task_tests_no_asan target.
#ifndef ASAN_ENABLED
TEST(TaskTest, DeepRecursiveAwaitExtremeDepthOptimized) {
  // Extreme depth to verify true tail call optimization works without ASAN overhead.
  // ASAN would prevent this from working, so this test only runs in no_asan build.
  constexpr int kDepth = 20000;
  const std::int64_t expected = static_cast<std::int64_t>(kDepth) * (kDepth + 1) / 2;

  Task<std::int64_t> task = DeepRecursiveSum(kDepth);

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), expected);
}
#endif

TEST(TaskVoidTest, SimpleResumeAndCompletion) {
  Task<void> task = VoidLeaf();

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
}

TEST(TaskVoidTest, SequentialAwaitOfTemporaryTasks) {
  Task<void> task = VoidSequence();

  EXPECT_FALSE(task.IsDone());

  task.Resume();

  EXPECT_TRUE(task.IsDone());
}

/////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////             SCHEDULER & YIELD TESTS              /////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

namespace {

std::vector<int> execution_order;

Task<void> SimpleYieldTask(int id) {
  execution_order.push_back(id * 10);
  co_await Yield();
  execution_order.push_back(id * 10 + 1);
  co_return;
}

Task<void> MultipleYieldsTask(int id) {
  execution_order.push_back(id * 100);
  co_await Yield();
  execution_order.push_back(id * 100 + 1);
  co_await Yield();
  execution_order.push_back(id * 100 + 2);
  co_return;
}

Task<void> NoYieldTask(int id) {
  execution_order.push_back(id * 100);
  execution_order.push_back(id * 100 + 1);
  execution_order.push_back(id * 100 + 2);
  co_return;
}

Task<void> StopTask(int id) {
  execution_order.push_back(id);
  co_await Yield();
  execution_order.push_back(id + 100);
  auto* sched = Scheduler::GetCurrentScheduler();
  if (sched) {
    sched->Stop();
  }
  co_return;
}

}  // namespace

TEST(SchedulerTest, SingleTaskWithSingleYield) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(SimpleYieldTask(1));
  scheduler.Run();

  EXPECT_EQ(execution_order, std::vector<int>({10, 11}));
}

TEST(SchedulerTest, TwoTasksWithYield) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(SimpleYieldTask(1));
  scheduler.Spawn(SimpleYieldTask(2));
  scheduler.Run();

  EXPECT_EQ(execution_order, std::vector<int>({10, 20, 11, 21}));
}

TEST(SchedulerTest, ThreeTasksRoundRobin) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(SimpleYieldTask(1));
  scheduler.Spawn(SimpleYieldTask(2));
  scheduler.Spawn(SimpleYieldTask(3));
  scheduler.Run();

  EXPECT_EQ(execution_order, std::vector<int>({10, 20, 30, 11, 21, 31}));
}

TEST(SchedulerTest, TaskWithMultipleYields) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(MultipleYieldsTask(1));
  scheduler.Run();

  EXPECT_EQ(execution_order, std::vector<int>({100, 101, 102}));
}

TEST(SchedulerTest, MixedYieldAndNoYieldTasks) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(SimpleYieldTask(1));
  scheduler.Spawn(NoYieldTask(2));
  scheduler.Spawn(SimpleYieldTask(3));
  scheduler.Run();

  // Task 1 yields, Task 2 runs to completion (no yield), Task 3 yields,
  // then Task 1 and Task 3 continue
  EXPECT_EQ(execution_order, std::vector<int>({10, 200, 201, 202, 30, 11, 31}));
}

TEST(SchedulerTest, StopSchedulerMidExecution) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(StopTask(1));
  scheduler.Spawn(SimpleYieldTask(2));
  scheduler.Run();

  // Task 1 yields at (1), then resumes and calls Stop()
  // Task 2 should start but scheduler stops before continuing
  EXPECT_EQ(execution_order, std::vector<int>({1, 20, 101, 21}));
}

TEST(SchedulerTest, ComplexInterleaving) {
  execution_order.clear();
  Scheduler scheduler;

  scheduler.Spawn(MultipleYieldsTask(1));
  scheduler.Spawn(SimpleYieldTask(2));
  scheduler.Spawn(MultipleYieldsTask(3));
  scheduler.Run();

  // Task 1: 100 -> yield
  // Task 2: 20 -> yield
  // Task 3: 300 -> yield
  // Task 1: 101 -> yield
  // Task 2: 21 (done)
  // Task 3: 301 -> yield
  // Task 1: 102 (done)
  // Task 3: 302 (done)
  EXPECT_EQ(execution_order, std::vector<int>({100, 20, 300, 101, 21, 301, 102, 302}));
}
