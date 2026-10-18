#include "AsyncSocket.hpp"

TCPSocketReadAwaiter::TCPSocketReadAwaiter(
  Scheduler* shed, asio::ip::tcp::socket& socket, asio::mutable_buffer buff):
  socket_(socket), buff_(buff), shed_(shed) {}

bool TCPSocketReadAwaiter::await_ready() { return false; }

void TCPSocketReadAwaiter::await_suspend(std::coroutine_handle<> outer) {
  auto cb = [&res = this->res_, shed = this->shed_, outer]
    (const asio::error_code& ec, size_t byte_transfered) {
    if (ec) {
      res = std::unexpected(ec);
    } else {
      res = byte_transfered;
    }
    shed->Schedule(outer);
    --shed->context_tasks_;
  };
  ++shed_->context_tasks_;
  socket_.async_read_some(buff_, cb);
}

size_t TCPSocketReadAwaiter::await_resume() {
  if (res_.has_value()) {
    return res_.value();
  }
  throw asio::system_error(res_.error());
}

TCPSocketWriteAwaiter::TCPSocketWriteAwaiter(
  Scheduler* shed, asio::ip::tcp::socket& socket, asio::const_buffer buff):
  socket_(socket), buff_(buff), shed_(shed) {}

bool TCPSocketWriteAwaiter::await_ready() { return false; }

void TCPSocketWriteAwaiter::await_suspend(std::coroutine_handle<> outer) {
  auto cb = [&res = this->res_, shed = this->shed_, outer] (const asio::error_code& ec, size_t byte_writed) {
    if (ec) {
      res = std::unexpected(ec);
    } else {
      res = byte_writed;
    }
    shed->Schedule(outer);
    --shed->context_tasks_;
  };
  ++shed_->context_tasks_;
  socket_.async_write_some(buff_, cb);
}

size_t TCPSocketWriteAwaiter::await_resume() {
  if (res_.has_value()) {
    return res_.value();
  }
  throw asio::system_error(res_.error());
}

TCPAcceptorWaiter::TCPAcceptorWaiter(asio::ip::tcp::acceptor& acceptor, Scheduler* shed):
  acceptor_(acceptor), shed_(shed) {}

bool TCPAcceptorWaiter::await_ready() { return false; }

void TCPAcceptorWaiter::await_suspend(std::coroutine_handle<> outer) {
  res_.emplace(AsyncTCPSocket(shed_, shed_->io_context_));

  auto cb = [&res = this->res_, shed = this->shed_, outer] (const asio::error_code& ec) {
    if (ec) {
      res = std::unexpected(ec);
    }
    shed->Schedule(outer);
    --shed->context_tasks_;
  };
  ++shed_->context_tasks_;
  acceptor_.async_accept(res_.value()->Socket(), cb);
}

AsyncTCPSocket TCPAcceptorWaiter::await_resume() {
  if (!res_.has_value()) {
    throw asio::system_error(res_.error());
  }
  return std::move(res_.value().value());
}

AsyncTCPSocket::AsyncTCPSocket(Scheduler* shed, asio::io_context& io_context):
  socket_(io_context), shed_(shed) {}

AsyncTCPSocket::AsyncTCPSocket(AsyncTCPSocket&& other) noexcept = default;

AsyncTCPSocket& AsyncTCPSocket::operator=(AsyncTCPSocket&& other) = default;

asio::ip::tcp::socket& AsyncTCPSocket::Socket() { return socket_; }

AsyncTCPAcceptor::AsyncTCPAcceptor(const asio::ip::tcp::endpoint& endpoint) {
  shed_ = Scheduler::GetCurrentScheduler();
  if (!shed_) {
    throw std::runtime_error("No current scheduler found");
  }
  acceptor_.emplace(asio::ip::tcp::acceptor(shed_->io_context_, endpoint));
}

AsyncTCPAcceptor::AsyncTCPAcceptor(AsyncTCPAcceptor&& other):
  acceptor_(std::move(other.acceptor_)), shed_(other.shed_) {}

AsyncTCPAcceptor& AsyncTCPAcceptor::operator=(AsyncTCPAcceptor&& other) {
  if (acceptor_.has_value()) {
    acceptor_->close();
  }
  acceptor_ = std::move(other.acceptor_);
  shed_ = other.shed_;

  return *this;
}

AsyncTCPSocket AsyncTCPAcceptor::Accept() {
  auto socket = AsyncTCPSocket(shed_, shed_->io_context_);
  acceptor_->accept(socket.Socket());
  return socket;
}

TCPAcceptorWaiter AsyncTCPAcceptor::AsyncAccept() { return {acceptor_.value(), shed_}; }

TCPSocketReadAwaiter AsyncTCPSocket::AsyncReadSome(asio::mutable_buffer buff) {
  return {shed_, socket_, buff};
}

TCPSocketWriteAwaiter AsyncTCPSocket::AsyncWriteSome(asio::const_buffer buff) {
  return {shed_, socket_, buff};
}

