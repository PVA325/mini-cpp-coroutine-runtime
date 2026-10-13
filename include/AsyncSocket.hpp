#pragma once
#include <expected>
#include <Scheduler.hpp>
#include <asio/ip/tcp.hpp>

class TCPSocketReadAwaiter {
public:
  TCPSocketReadAwaiter(Scheduler* shed, asio::ip::tcp::socket& socket, asio::mutable_buffer buff):
    socket_(socket), buff_(buff), shed_(shed) {}

  bool suspend_ready() { return false; }
  void suspend_wait(std::coroutine_handle<> outer);
  size_t suspend_resume();

private:
  asio::ip::tcp::socket& socket_;
  asio::mutable_buffer buff_;
  Scheduler* shed_;
  std::expected<size_t, asio::error_code> res_{0};
};

class TCPSocketWriteAwaiter {
public:
  TCPSocketWriteAwaiter(Scheduler* shed, asio::ip::tcp::socket& socket, asio::const_buffer buff):
    socket_(socket), buff_(buff), shed_(shed) {}

  bool suspend_ready() { return false; }
  void suspend_wait(std::coroutine_handle<> outer);
  size_t suspend_resume();

private:
  asio::ip::tcp::socket& socket_;
  asio::const_buffer buff_;
  Scheduler* shed_;
  std::expected<size_t, asio::error_code> res_{0};
};

class AsyncTCPSocket {
public:
  AsyncTCPSocket(Scheduler* shed, asio::io_context& io_context): socket_(io_context), shed_(shed) {}

  AsyncTCPSocket(const AsyncTCPSocket&) = delete;
  AsyncTCPSocket& operator=(const AsyncTCPSocket&) = delete;
  AsyncTCPSocket(AsyncTCPSocket&& other) = default;
  AsyncTCPSocket& operator=(AsyncTCPSocket&& other) = default;

  TCPSocketReadAwaiter AsyncReadSome(asio::mutable_buffer buff);
  TCPSocketWriteAwaiter AsyncWriteSome(asio::const_buffer buff);

  asio::ip::tcp::socket& Socket() { return socket_; }
private:
  asio::ip::tcp::socket socket_;
  Scheduler* shed_;
};

class TCPAcceptorWaiter {
public:
  TCPAcceptorWaiter(asio::ip::tcp::acceptor& acceptor, Scheduler* shed):
    acceptor_(acceptor), shed_(shed) {}

  bool suspend_ready() { return false; }
  void suspend_wait(std::coroutine_handle<> outer);
  AsyncTCPSocket suspend_resume();
private:
  asio::ip::tcp::acceptor& acceptor_;
  Scheduler* shed_;
  std::expected<std::optional<AsyncTCPSocket>, asio::error_code> res_{std::nullopt};
};

class AsyncTCPAcceptor {
public:
  AsyncTCPAcceptor(const asio::ip::tcp::endpoint& endpoint);

  AsyncTCPAcceptor(const AsyncTCPAcceptor&) = delete;
  AsyncTCPAcceptor& operator=(const AsyncTCPAcceptor&) = delete;
  AsyncTCPAcceptor(AsyncTCPAcceptor&& other);
  AsyncTCPAcceptor& operator=(AsyncTCPAcceptor&& other);

  AsyncTCPSocket Accept();
  TCPAcceptorWaiter AsyncAccept();

private:
  std::optional<asio::ip::tcp::acceptor> acceptor_;
  Scheduler* shed_;
};
