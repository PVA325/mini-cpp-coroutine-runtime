#include <gtest/gtest.h>
#include <Scheduler.hpp>
#include <SleepFor.hpp>
#include <Task.hpp>
#include <Yield.hpp>

#include <chrono>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;

Task<void> SleepAndRecord(std::chrono::milliseconds delay, bool& resumed,
                          Clock::time_point& completion) {
  co_await SleepFor(delay);
  completion = Clock::now();
  resumed = true;
}

Task<void> SleepWithPeer(std::vector<int>& order) {
  order.push_back(1);
  co_await SleepFor(20ms);
  order.push_back(3);
}
Task<void> ReadyPeer(std::vector<int>& order) {
  order.push_back(2);
  co_return;
}
}  // namespace

TEST(SleepForTest, DoesNotResumeBeforeTimerCompletion) {
  Scheduler scheduler;
  bool resumed = false;
  Clock::time_point completion{};
  const auto started = Clock::now();
  scheduler.Spawn(SleepAndRecord(30ms, resumed, completion));
  scheduler.Run();
  ASSERT_TRUE(resumed);
  EXPECT_GE(completion - started, 30ms);
}

TEST(SleepForTest, RunWaitsWhenTimerIsOnlyPendingWork) {
  Scheduler scheduler;
  bool resumed = false;
  Clock::time_point completion{};
  scheduler.Spawn(SleepAndRecord(10ms, resumed, completion));
  scheduler.Run();
  EXPECT_TRUE(resumed);
}

TEST(SleepForTest, OtherReadyTaskRunsWhileTimerIsPending) {
  Scheduler scheduler;
  std::vector<int> order;
  scheduler.Spawn(SleepWithPeer(order));
  scheduler.Spawn(ReadyPeer(order));
  scheduler.Run();
  EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}
