
#define BOOST_ASIO_HAS_MOVE 1
#define BOOST_ASIO_ERROR_CATEGORY_NOEXCEPT noexcept(true)
#define _WIN32_WINNT 0x0501

#include <boost/asio.hpp>
#include <iostream>

namespace game_server {

using socket =  boost::asio::ip::tcp::socket;
using io_service = boost::asio::io_service;
using tcp = boost::asio::ip::tcp;

class connection {
public:
  connection(io_service &io, socket &sock) :
    io_(io),
    socket_(std::move(sock)),
    is_stopped(false)
  {
    std::cout << "accepted " << socket_.native_handle() << "\n";
    do_read();
  }
private:
  bool parse_header() {
    bool seen_end = false;
    bool seen_get = false;
    auto p = header_.begin();
    auto e = header_.end();
    for (; p != e; ) {
      auto b = p;
      while (p != e && *p != '\n') ++p;
      if (p != e) ++p;
      if (is(b, e, "\r\n")) {
        seen_end = true;
        break;
      } else if (is(b, p, "GET ")) {
        b += 4;
        url_.clear();
        while (b != p && *b != ' ') url_.push_back(*b++);
        if (is(b, p, " HTTP/1.1\r\n")) {
          seen_get = true;
        }
      }
    }
    if (seen_end) {
      std::cout << std::string(header_.begin(), p);
      header_.assign(p, e);
      return true;
    }
    return false;
  }

  void do_read() {
    socket_.async_read_some(boost::asio::buffer(buffer_),
      [this](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (!ec) {
          std::cout << socket_.native_handle() << " read " << bytes_transferred << "\n";
          header_.append(buffer_.data(), buffer_.data() + bytes_transferred);
          if (parse_header()) {
          }
          do_read();
        }
        else if (ec != boost::asio::error::operation_aborted)
        {
          std::cout << socket_.native_handle() << " closed\n";
          is_stopped = true;
        } else {
          do_read();
        }
      }
    );
  }

  bool is(std::string::iterator b, std::string::iterator e, const char *str) {
    while (*str) {
      if (b == e) return false;
      if (*b != *str) return false;
      ++str;
      ++b;
    }
    return true;
  }

  /// The io_service used to perform asynchronous operations.
  boost::asio::io_service &io_;

  std::array<char, 32> buffer_;

  socket socket_;
  std::string header_;
  std::string url_;

  bool is_stopped;
};

class server {
public:
  server(boost::asio::io_service &io) :
    io_(io),
    acceptor_(io, tcp::endpoint(tcp::v4(), 8000)),
    socket_(io)
  {
    do_accept();
  }

  void do_accept() {
    acceptor_.async_accept(socket_,
      [this](boost::system::error_code ec)
      {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!acceptor_.is_open())
        {
          return;
        }

        if (!ec)
        {
          connections.push_back(new connection(io_, std::move(socket_)));
        }

        do_accept();
      }
    );
  }
private:
  std::vector<connection *> connections;

  /// The io used to perform asynchronous operations.
  io_service &io_;

  /// Acceptor used to listen for incoming connections.
  boost::asio::ip::tcp::acceptor acceptor_;

  /// The next socket to be accepted.
  socket socket_;
};

}


int main() {
  boost::asio::io_service io;

  game_server::server svr(io);
  io.run();
}
