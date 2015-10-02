
#define BOOST_ASIO_HAS_MOVE 1
#define BOOST_ASIO_ERROR_CATEGORY_NOEXCEPT noexcept(true)
#define _WIN32_WINNT 0x0501

#include <boost/asio.hpp>
#include <iostream>
#include <fstream>

namespace game_server {

using socket =  boost::asio::ip::tcp::socket;
using io_service = boost::asio::io_service;
using tcp = boost::asio::ip::tcp;

class connection {
public:
  connection(io_service &io, socket &sock) :
    io_(io),
    socket_(std::move(sock))
  {
    std::cout << "accepted " << socket_.native_handle() << "\n";
    do_read();
  }

  bool is_open() {
    return socket_.is_open();
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
    std::cout << "do_read " << socket_.native_handle() << "\n";
    socket_.async_read_some(boost::asio::buffer(buffer_),
      [this](boost::system::error_code ec, std::size_t bytes_transferred) {
        std::cout << "read " << socket_.native_handle() << " err=" << ec << "\n";
        if (!ec) {
          //std::cout << socket_.native_handle() << " read " << bytes_transferred << "\n";
          header_.append(buffer_.data(), buffer_.data() + bytes_transferred);
          if (parse_header()) {
            bool error = true;
            if (url_.find_first_of("..", 2) == url_.npos) {
              if (!url_.empty() && url_.back() == '/') {
                url_.append("index.html");
              }
              std::ifstream is("htdocs" + url_);
              if (!is.fail()) {
                std::string file;
                while (!is.eof()) {
                  size_t size = file.size();
                  is.read(buffer_.data(), buffer_.size());
                  size_t extra = (size_t)is.gcount();
                  file.append(buffer_.begin(), buffer_.begin() + extra);
                }
                send_http("HTTP/1.0 200 OK\r\n", file);
                error = false;
              }
            }
            if (error) {
              send_http("HTTP/1.0 404 Not Found\r\n", "<html><body>Not found</body></html>");
            }
            do_write();
          }
        } else if (ec != boost::asio::error::operation_aborted) {
          std::cout << "close " << socket_.native_handle() << "\n";
          socket_.close();
        }
      }
    );
  }

  void do_write() {
    std::cout << "do_write " << socket_.native_handle() << " sz=" << buffers_.size() << "\n";
    boost::asio::async_write(
      socket_,
      buffers_,
      [this](boost::system::error_code ec, std::size_t bytes_transferred) {
        std::cout << "write " << socket_.native_handle() << " err=" << ec << "\n";
        if (!ec) {
          //do_write();
        } else {
          //socket_.close();
        }
        do_read();
      }
    );
  }

  void send_http(const char *code, const std::string &str) {
    response_.clear();
    response_.append(code);
    response_.append("Content-Length: ");
    response_.append(std::to_string(str.size()));
    response_.append("\r\n");
    response_.append("Content-Type: text/html\r\n");
    response_.append("\r\n");
    response_.append(str);
    buffers_.clear();
    buffers_.emplace_back(boost::asio::buffer(response_));
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

  std::array<char, 2048> buffer_;

  socket socket_;
  std::string header_;
  std::string url_;
  std::string response_;
  std::vector<boost::asio::const_buffer> buffers_;
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
        if (acceptor_.is_open() && !ec) {
          size_t i = 0, j = 0;
          for (; i != connections.size(); ++i) {
            std::cout << i << " " << connections[i]->is_open() << "\n";
            if (connections[i]->is_open()) {
              connections[j++] = connections[i];
            } else {
              delete connections[i];
            }
          }
          connections.resize(j);
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
