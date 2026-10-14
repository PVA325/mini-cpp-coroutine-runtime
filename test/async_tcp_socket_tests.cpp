#include <gtest/gtest.h>
#include <AsyncSocket.hpp>
#include <Scheduler.hpp>
#include <Task.hpp>

#include <asio/buffer.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/write.hpp>
#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>
#include <type_traits>

namespace {
using asio::ip::tcp;
using namespace std::chrono_literals;

unsigned short FreeLoopbackPort() {
  asio::io_context context;
  tcp::acceptor probe(context, tcp::endpoint(asio::ip::address_v4::loopback(), 0));
  return probe.local_endpoint().port();
}

tcp::socket ConnectWithRetry(asio::io_context& context, unsigned short port) {
  tcp::socket socket(context);
  const tcp::endpoint endpoint(asio::ip::address_v4::loopback(), port);
  const auto deadline = std::chrono::steady_clock::now() + 1s;
  while (std::chrono::steady_clock::now() < deadline) {
    asio::error_code error;
    socket.close(error);
    socket.connect(endpoint, error);
    if (!error) return socket;
    std::this_thread::sleep_for(2ms);
  }
  asio::error_code error;
  socket.close(error);
  return socket;
}

Task<void> ReadOnce(unsigned short port, std::string& received, size_t& count) {
  AsyncTCPAcceptor acceptor(tcp::endpoint(asio::ip::address_v4::loopback(), port));
  auto socket = co_await acceptor.AsyncAccept();
  std::array<char, 32> buffer{};  // Lives in this coroutine frame until read completion.
  count = co_await socket.AsyncReadSome(asio::buffer(buffer));
  received.assign(buffer.data(), count);
}

Task<void> WriteOnce(unsigned short port, size_t& count) {
  AsyncTCPAcceptor acceptor(tcp::endpoint(asio::ip::address_v4::loopback(), port));
  auto socket = co_await acceptor.AsyncAccept();
  const std::string message = "hello";  // Lives until write completion.
  count = co_await socket.AsyncWriteSome(asio::buffer(message));
}

enum class Operation { Read, Write };
Task<void> ClosedSocketError(unsigned short port, Operation operation, bool& caught) {
  AsyncTCPAcceptor acceptor(tcp::endpoint(asio::ip::address_v4::loopback(), port));
  auto socket = co_await acceptor.AsyncAccept();
  socket.Socket().close();
  std::array<char, 8> buffer{};
  try {
    if (operation == Operation::Read) {
      (void)co_await socket.AsyncReadSome(asio::buffer(buffer));
    } else {
      (void)co_await socket.AsyncWriteSome(asio::buffer(buffer));
    }
  } catch (const asio::system_error&) {
    caught = true;
  }
}
}  // namespace

static_assert(!std::is_copy_constructible_v<AsyncTCPSocket>);
static_assert(!std::is_copy_assignable_v<AsyncTCPSocket>);
static_assert(std::is_move_constructible_v<AsyncTCPSocket>);

TEST(AsyncTCPSocketTest, ReadResumesAfterDataAndReturnsByteCount) {
  Scheduler scheduler;
  const auto port = FreeLoopbackPort();
  std::string received;
  size_t count = 0;
  std::atomic_bool connected = false;
  std::jthread client([&] {
    asio::io_context context;
    auto socket = ConnectWithRetry(context, port);
    if (!socket.is_open()) return;
    connected = true;
    asio::error_code error;
    (void)asio::write(socket, asio::buffer("hello", 5), error);
  });
  scheduler.Spawn(ReadOnce(port, received, count));
  scheduler.Run();
  client.join();
  EXPECT_TRUE(connected);
  EXPECT_GT(count, 0u);
  EXPECT_LE(count, 5u);
  EXPECT_EQ(received, std::string("hello").substr(0, count));
}

TEST(AsyncTCPSocketTest, WriteReturnsTransferredCount) {
  Scheduler scheduler;
  const auto port = FreeLoopbackPort();
  size_t count = 0;
  std::string received;
  std::jthread client([&] {
    asio::io_context context;
    auto socket = ConnectWithRetry(context, port);
    if (!socket.is_open()) return;
    std::array<char, 32> buffer{};
    asio::error_code error;
    for (;;) {
      const auto n = socket.read_some(asio::buffer(buffer), error);
      received.append(buffer.data(), n);
      if (error) break;
    }
  });
  scheduler.Spawn(WriteOnce(port, count));
  scheduler.Run();
  client.join();
  EXPECT_GT(count, 0u);
  EXPECT_LE(count, 5u);
  EXPECT_EQ(received, std::string("hello").substr(0, count));
}

TEST(AsyncTCPSocketTest, ReadErrorResumesCoroutineAndReportsError) {
  Scheduler scheduler;
  const auto port = FreeLoopbackPort();
  bool caught = false;
  std::jthread client([&] { asio::io_context context; auto socket = ConnectWithRetry(context, port); });
  scheduler.Spawn(ClosedSocketError(port, Operation::Read, caught));
  scheduler.Run();
  client.join();
  EXPECT_TRUE(caught);
}

TEST(AsyncTCPSocketTest, WriteErrorResumesCoroutineAndReportsError) {
  Scheduler scheduler;
  const auto port = FreeLoopbackPort();
  bool caught = false;
  std::jthread client([&] { asio::io_context context; auto socket = ConnectWithRetry(context, port); });
  scheduler.Spawn(ClosedSocketError(port, Operation::Write, caught));
  scheduler.Run();
  client.join();
  EXPECT_TRUE(caught);
}
