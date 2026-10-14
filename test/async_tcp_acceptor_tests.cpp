#include <gtest/gtest.h>
#include <AsyncSocket.hpp>
#include <Scheduler.hpp>
#include <Task.hpp>

#include <asio/ip/tcp.hpp>
#include <atomic>
#include <chrono>
#include <optional>
#include <thread>
#include <type_traits>
#include <vector>

namespace {
using asio::ip::tcp;
using namespace std::chrono_literals;

unsigned short FreeLoopbackPort() {
  asio::io_context context;
  tcp::acceptor probe(context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
  return probe.local_endpoint().port();
}

std::jthread ConnectOnce(unsigned short port, std::atomic_bool& connected) {
  return std::jthread([port, &connected] {
    asio::io_context context;
    tcp::socket client(context);
    const tcp::endpoint endpoint(asio::ip::address_v4::loopback(), port);
    const auto deadline = std::chrono::steady_clock::now() + 1s;
    while (std::chrono::steady_clock::now() < deadline) {
      asio::error_code error;
      client.close(error);
      client.connect(endpoint, error);
      if (!error) { connected = true; return; }
      std::this_thread::sleep_for(2ms);
    }
  });
}

Task<void> AcceptAndRecord(unsigned short port, std::vector<int>& order,
                           std::optional<AsyncTCPSocket>& accepted) {
  AsyncTCPAcceptor acceptor(tcp::endpoint(asio::ip::address_v4::loopback(), port));
  order.push_back(1);
  accepted.emplace(co_await acceptor.AsyncAccept());
  order.push_back(3);
}
Task<void> ReadyTask(std::vector<int>& order) { order.push_back(2); co_return; }
}  // namespace

static_assert(!std::is_copy_constructible_v<AsyncTCPAcceptor>);
static_assert(!std::is_copy_assignable_v<AsyncTCPAcceptor>);
static_assert(std::is_move_constructible_v<AsyncTCPAcceptor>);

TEST(AsyncTCPAcceptorTest, AsyncAcceptSuspendsWithoutBlockingReadyTasks) {
  Scheduler scheduler;
  std::vector<int> order;
  std::optional<AsyncTCPSocket> accepted;
  std::atomic_bool connected = false;
  const auto port = FreeLoopbackPort();
  auto client = ConnectOnce(port, connected);
  scheduler.Spawn(AcceptAndRecord(port, order, accepted));
  scheduler.Spawn(ReadyTask(order));
  scheduler.Run();
  EXPECT_TRUE(connected);
  EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(AsyncTCPAcceptorTest, AcceptedSocketSurvivesAcceptorDestruction) {
  Scheduler scheduler;
  std::vector<int> order;
  std::optional<AsyncTCPSocket> accepted;
  std::atomic_bool connected = false;
  const auto port = FreeLoopbackPort();
  auto client = ConnectOnce(port, connected);
  scheduler.Spawn(AcceptAndRecord(port, order, accepted));
  scheduler.Run();
  ASSERT_TRUE(accepted.has_value());
  EXPECT_TRUE(accepted->Socket().is_open());
}
