#include "AsyncSocket.hpp"

void TCPSocketReadAwaiter::suspend_wait(std::coroutine_handle<> outer) {
  auto cb = [&res = this->res_, shed = this->shed_, outer]
    (const asio::error_code& ec, size_t byte_transfered) {
    if (ec) {
      res = std::unexpected(ec);
      return;
    }
    res = byte_transfered;
    shed->Schedule(outer);
  };
  socket_.async_read_some(buff_, cb);
}

size_t TCPSocketReadAwaiter::suspend_resume() {
  if (res_.has_value()) {
    return res_.value();
  }
  throw asio::system_error(res_.error());
}

void TCPSocketWriteAwaiter::suspend_wait(std::coroutine_handle<> outer) {
  auto cb = [&res = this->res_, shed = this->shed_, outer] (const asio::error_code& ec, size_t byte_writed) {
    if (ec) {
      res = std::unexpected(ec);
      return;
    }
    res = byte_writed;
    shed->Schedule(outer);
  };
  socket_.async_write_some(buff_, cb);
}

size_t TCPSocketWriteAwaiter::suspend_resume() {
  if (res_.has_value()) {
    return res_.value();
  }
  throw asio::system_error(res_.error());
}

void TCPAcceptorWaiter::suspend_wait(std::coroutine_handle<> outer) {
  res_.emplace(AsyncTCPSocket(shed_, shed_->io_context_));

  auto cb = [&res = this->res_, shed = this->shed_, outer] (const asio::error_code& ec) {
    if (ec) {
      res = std::unexpected(ec);
      return;
    }
    shed->Schedule(outer);
  };
  acceptor_.async_accept(res_.value()->Socket(), cb);
}

AsyncTCPSocket TCPAcceptorWaiter::suspend_resume() {
  if (!res_.has_value()) {
    throw asio::system_error(res_.error());
  }
  return std::move(res_.value().value());
}

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

TCPSocketReadAwaiter AsyncTCPSocket::AsyncReadSome(asio::mutable_buffer buff) {
  return {shed_, socket_, buff};
}

TCPSocketWriteAwaiter AsyncTCPSocket::AsyncWriteSome(asio::const_buffer buff) {
  return {shed_, socket_, buff};
}

