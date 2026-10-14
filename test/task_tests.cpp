#include <gtest/gtest.h>
#include <Task.hpp>

#include <coroutine>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace {
Task<int> Value(int n) { co_return n; }
Task<int> Sequence() { int n = co_await Value(2); co_return (co_await Value(n)) + 1; }
Task<std::int64_t> Sum(int n) {
  if (n == 0) co_return 0;
  co_return (co_await Sum(n - 1)) + n;
}
Task<void> VoidLeaf() { co_return; }
Task<void> VoidSequence() { co_await VoidLeaf(); co_await VoidLeaf(); }
// A coroutine parameter stays in its frame until the frame is destroyed.
Task<void> Hold(std::shared_ptr<int> token) { (void)token; co_await std::suspend_always{}; }
Task<int> Child(std::shared_ptr<int> token) { (void)token; co_return 42; }
Task<int> AwaitMoved(Task<int>& child, bool& empty) {
  int value = co_await std::move(child);
  empty = !child.Handler();
  co_return value;
}
Task<int> ThrowValue() { throw std::runtime_error("value"); co_return 0; }
Task<int> AwaitThrowValue() { co_return co_await ThrowValue(); }
Task<void> ThrowVoid() { throw std::runtime_error("void"); co_return; }
Task<bool> CatchVoid() {
  try { co_await ThrowVoid(); }
  catch (const std::exception_ptr& error) {
    try { std::rethrow_exception(error); }
    catch (const std::runtime_error& e) { co_return std::string(e.what()) == "void"; }
  }
  co_return false;
}
}  // namespace

static_assert(!std::is_copy_constructible_v<Task<int>>);
static_assert(!std::is_copy_assignable_v<Task<int>>);
static_assert(std::is_move_constructible_v<Task<int>>);
static_assert(!std::is_copy_constructible_v<Task<void>>);

TEST(TaskTest, StartsSuspendedAndReturnsValueAfterResume) {
  auto task = Value(42);
  EXPECT_FALSE(task.IsDone());
  task.Resume();
  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), 42);
}

TEST(TaskTest, ManualOwnerDestroysSuspendedFrame) {
  auto token = std::make_shared<int>(1);
  std::weak_ptr<int> weak = token;
  {
    auto task = Hold(std::move(token));
    task.Resume();
    EXPECT_FALSE(task.IsDone());
    EXPECT_FALSE(weak.expired());
  }
  EXPECT_TRUE(weak.expired());
}

TEST(TaskTest, MoveConstructorTransfersOwnership) {
  auto token = std::make_shared<int>(1);
  std::weak_ptr<int> weak = token;
  {
    auto source = Hold(std::move(token));
    auto owner = std::move(source);
    EXPECT_FALSE(source.Handler());
    EXPECT_TRUE(owner.Handler());
    EXPECT_FALSE(weak.expired());
  }
  EXPECT_TRUE(weak.expired());
}

TEST(TaskTest, MoveAssignmentDestroysPreviousFrame) {
  auto old_token = std::make_shared<int>(1);
  auto new_token = std::make_shared<int>(2);
  std::weak_ptr<int> old_weak = old_token, new_weak = new_token;
  {
    auto destination = Hold(std::move(old_token));
    auto source = Hold(std::move(new_token));
    destination = std::move(source);
    EXPECT_TRUE(old_weak.expired());
    EXPECT_FALSE(new_weak.expired());
    EXPECT_FALSE(source.Handler());
  }
  EXPECT_TRUE(new_weak.expired());
}

TEST(TaskTest, AwaitedTemporaryReturnsThroughContinuation) {
  auto task = Sequence();
  task.Resume();
  EXPECT_TRUE(task.IsDone());
  EXPECT_EQ(task.GetValue(), 3);
}

TEST(TaskTest, AwaitMovesHandleAndDestroysChildFrame) {
  auto token = std::make_shared<int>(1);
  std::weak_ptr<int> weak = token;
  auto child = Child(std::move(token));
  bool empty = false;
  auto parent = AwaitMoved(child, empty);
  parent.Resume();
  EXPECT_TRUE(parent.IsDone());
  EXPECT_TRUE(empty);
  EXPECT_FALSE(child.Handler());
  EXPECT_TRUE(weak.expired());
  EXPECT_EQ(parent.GetValue(), 42);
}

TEST(TaskTest, ValueExceptionPropagatesThroughGetValueAndAwait) {
  auto direct = ThrowValue();
  direct.Resume();
  EXPECT_THROW(direct.GetValue(), std::runtime_error);
  auto parent = AwaitThrowValue();
  parent.Resume();
  EXPECT_THROW(parent.GetValue(), std::runtime_error);
}

TEST(TaskTest, DeepAwaitChainUsesSymmetricTransfer) {
  constexpr int depth = 1000;
  auto task = Sum(depth);
  task.Resume();
  EXPECT_EQ(task.GetValue(), static_cast<std::int64_t>(depth) * (depth + 1) / 2);
}
#ifndef ASAN_ENABLED
TEST(TaskTest, ExtremeDeepAwaitChainOptimized) {
  constexpr int depth = 20000;
  auto task = Sum(depth);
  task.Resume();
  EXPECT_EQ(task.GetValue(), static_cast<std::int64_t>(depth) * (depth + 1) / 2);
}
#endif

TEST(TaskVoidTest, CompletesAfterManualResume) {
  auto task = VoidLeaf();
  EXPECT_FALSE(task.IsDone());
  task.Resume();
  EXPECT_TRUE(task.IsDone());
}
TEST(TaskVoidTest, AwaitsTemporaryChildren) {
  auto task = VoidSequence();
  task.Resume();
  EXPECT_TRUE(task.IsDone());
}
TEST(TaskVoidTest, AwaitedExceptionUsesDocumentedExceptionPtrBehavior) {
  auto task = CatchVoid();
  task.Resume();
  EXPECT_TRUE(task.GetValue());
}
