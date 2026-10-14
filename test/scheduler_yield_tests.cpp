#include <gtest/gtest.h>
#include <Scheduler.hpp>
#include <Task.hpp>
#include <Yield.hpp>

#include <memory>
#include <utility>
#include <vector>

namespace {
Task<void> YieldOnce(std::vector<int>& order, int id) {
  order.push_back(id * 10);
  co_await Yield();
  order.push_back(id * 10 + 1);
}
Task<void> YieldTwice(std::vector<int>& order, int id) {
  order.push_back(id * 100);
  co_await Yield();
  order.push_back(id * 100 + 1);
  co_await Yield();
  order.push_back(id * 100 + 2);
}
Task<void> NoYield(std::vector<int>& order) {
  order.push_back(200);
  order.push_back(201);
  co_return;
}
Task<void> Hold(std::shared_ptr<int> token, bool& ran) {
  (void)token;
  ran = true;
  co_return;
}
}  // namespace

TEST(SchedulerTest, SpawnTransfersOwnershipAndDefersExecution) {
  Scheduler scheduler;
  bool ran = false;
  auto token = std::make_shared<int>(1);
  std::weak_ptr<int> weak = token;
  {
    auto source = Hold(std::move(token), ran);
    scheduler.Spawn(std::move(source));
    EXPECT_FALSE(source.Handler());
    EXPECT_FALSE(ran);
    EXPECT_FALSE(weak.expired());
  }
  scheduler.Run();
  EXPECT_TRUE(ran);
  EXPECT_TRUE(weak.expired());
}

TEST(SchedulerTest, YieldRequeuesCurrentCoroutine) {
  Scheduler scheduler;
  std::vector<int> order;
  scheduler.Spawn(YieldOnce(order, 1));
  scheduler.Spawn(YieldOnce(order, 2));
  scheduler.Run();
  EXPECT_EQ(order, (std::vector<int>{10, 20, 11, 21}));
}

TEST(SchedulerTest, MultipleYieldsInterleaveReadyTasks) {
  Scheduler scheduler;
  std::vector<int> order;
  scheduler.Spawn(YieldTwice(order, 1));
  scheduler.Spawn(YieldOnce(order, 2));
  scheduler.Spawn(YieldTwice(order, 3));
  scheduler.Run();
  EXPECT_EQ(order, (std::vector<int>{100, 20, 300, 101, 21, 301, 102, 302}));
}

TEST(SchedulerTest, TaskWithoutYieldRunsUntilCompletion) {
  Scheduler scheduler;
  std::vector<int> order;
  scheduler.Spawn(YieldOnce(order, 1));
  scheduler.Spawn(NoYield(order));
  scheduler.Spawn(YieldOnce(order, 3));
  scheduler.Run();
  EXPECT_EQ(order, (std::vector<int>{10, 200, 201, 30, 11, 31}));
}

TEST(SchedulerTest, CompletedTopLevelFrameIsDestroyedBeforeRunReturns) {
  Scheduler scheduler;
  bool ran = false;
  auto token = std::make_shared<int>(1);
  std::weak_ptr<int> weak = token;
  scheduler.Spawn(Hold(std::move(token), ran));
  scheduler.Run();
  EXPECT_TRUE(ran);
  EXPECT_TRUE(weak.expired());
}
