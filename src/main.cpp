#include <iostream>
#include <Task.hpp>
#include <Scheduler.hpp>
#include <Yield.hpp>
#include <SleepFor.hpp>
#include <AsyncSocket.hpp>
#include <asio/ip/tcp.hpp>
#include <format>

class Session : std::enable_shared_from_this<Session> {
public:
  Session(AsyncTCPSocket&& socket) : socket_(std::move(socket)) {}

  Task<void> Run(std::shared_ptr<Session> self) {
    while (true) {
      size_t readed;
      try {
        readed = co_await socket_.AsyncReadSome(asio::buffer(buff_));
      } catch (...) {
        asio::ip::tcp::endpoint endpoint = socket_.Socket().remote_endpoint();
        std::cerr << std::format("[[Session::Run::AsyncReadSome]] caught an exception on remote: {}:{}\n",
          endpoint.address().to_string(), endpoint.port());
        break;
      }
      size_t writed = 0;
      while (writed < readed) {
        auto buffer_slice = asio::buffer(buff_ + writed, readed - writed);
        try {
          writed += co_await socket_.AsyncWriteSome(buffer_slice);
        } catch (...) {
          asio::ip::tcp::endpoint endpoint = socket_.Socket().remote_endpoint();
          std::cerr << std::format("[[Session::Run::AsyncWriteSome]] caught an exception on remote: {}:{}\n",
            endpoint.address().to_string(), endpoint.port());
          break;
        }
      }
    }
  }
private:
  AsyncTCPSocket socket_;
  char buff_[1024];
};

int main() {
  Scheduler scheduler;
  auto life = [] () -> Task<void> {
    asio::ip::address ip_addr = asio::ip::address::from_string("127.0.0.1");
    asio::ip::tcp::endpoint endpoint(ip_addr, 5555);
    AsyncTCPAcceptor acceptor(endpoint);
    while (true) {
      AsyncTCPSocket socket = co_await acceptor.AsyncAccept();
      auto session = std::make_shared<Session>(std::move(socket));
      Scheduler::GetCurrentScheduler()->Spawn(session->Run(session));
    }
  };
  scheduler.Spawn(std::move(life()));
  scheduler.Run();
}