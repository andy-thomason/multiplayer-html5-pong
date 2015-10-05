#include <cctype>

namespace game_server {

  using boost::asio::io_service;
  using socket = boost::asio::ip::tcp::socket;

  class connection {
  public:
    /// create a new connection from an accepted socket.
    connection(io_service &io, socket &sock, game *_game) :
      io_(io),
      socket_(std::move(sock)),
      timer_(io),
      game_(_game)
    {
      std::cout << "accepted " << socket_.native_handle() << "\n";
      do_tick();
    }

    /// return true if the socket is still open.
    bool is_open() {
      return socket_.is_open();
    }
  private:
    // parse all or part of an html request header
    // return false if we are still reading (unlikely).
    bool parse_http() {
      auto p = header_.begin();
      auto e = header_.end();
      size_t length = 0;
      url_.clear();

      if (logging) std::cout << header_ << "\n";

      // scan lines of the header
      for (; p != e; ) {
        auto b = p;
        while (p != e && *p != '\n') ++p;
        if (p != e) ++p;

        if (is(b, e, "\r\n") && p + length >= e) {
          // blank line: end of header
          data_.assign(p, p + length);
          header_.assign(p + length, e);
          return true;
        } else if (is(b, p, "Content-Length: ")) {
          b += 16;
          length = 0;
          while (b != p && std::isdigit(*b)) length = length * 10 + *b++ - '0';
        } else if (is(b, p, "PUT ") || is(b, p, "GET ")) {
          // PUT /url HTTP/1.1
          b += 4;
          url_.clear();
          while (b != p && *b != ' ') url_.push_back(*b++);
        }
      }
      if (logging) std::cout << "not well formed\n";
      return false;
    }

    // initiate and handle a read request.
    // the lambda will live on after do_read has returned.
    void do_read() {
      if (!socket_.is_open()) return;

      //if (logging) std::cout << "do_read " << socket_.native_handle() << "\n";
      socket_.async_read_some(boost::asio::buffer(buffer_),
        [this](boost::system::error_code ec, std::size_t bytes_transferred) {
          if (logging) std::cout << "read " << socket_.native_handle() << " err=" << ec << "\n";
          if (!ec) {
            if (logging) std::cout << socket_.native_handle() << " read " << bytes_transferred << "\n";
            header_.append(buffer_.data(), buffer_.data() + bytes_transferred);
            if (parse_http()) {
              if (!send_response()) {
                send_http("HTTP/1.1 404 Not Found\r\n", "text/html", "<html><body>Not found</body></html>");
              }
              do_write();
            }
          } else if (socket_.is_open()) {
            socket_.close();
          }
        }
      );
    }

    // initiate and handle a write request.
    // the lambda will live on after do_write has returned.
    void do_write() {
      if (logging) std::cout << "do_write " << socket_.native_handle() << " sz=" << buffers_.size() << "\n";
      boost::asio::async_write(
        socket_,
        buffers_,
        [this](boost::system::error_code ec, std::size_t bytes_transferred) {
          // do nothing.
        }
      );
    }

    // Game timer tick. Issues a read request every 33 ms or so.
    void do_tick() {
      timer_.expires_from_now(boost::posix_time::milliseconds(33));
      timer_.async_wait(
        [this](const boost::system::error_code &ec) {
          if (!ec) {
            do_read();
            do_tick();
          }
        }
      );
    }

    // set a simple HTTP response. Used for files, errors and the game data.
    void send_http(const char *code, const char *mime, const std::string &str) {
      // note we do not create a new string every time!
      response_.clear();
      response_.append(code);
      response_.append("Content-Length: ");
      response_.append(std::to_string(str.size()));
      response_.append("\r\nContent-Type: ");
      response_.append(mime);
      response_.append("\r\n\r\n");
      response_.append(str);
      buffers_.clear();
      buffers_.emplace_back(boost::asio::buffer(response_));
    }

    bool send_response() {
      if (logging) std::cout << "url=" << url_ << "\n";

      if (url_ == "/data") {
        if (logging) std::cout << "tick\n";

        // run a frame of the game and write
        game_->do_frame(data_);

        data_.clear();
        game_->write(data_);

        send_http("HTTP/1.1 200 OK\r\n", "application/octet-stream", data_);
        return true;
      }

      if (url_.find_first_of("..", 2) != url_.npos) return false;

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
        send_http("HTTP/1.1 200 OK\r\n", "text/html", file);
        return true;
      }
      return false;
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

    /// client socket from accept()
    socket socket_;

    /// timer used to initiate reads
    boost::asio::deadline_timer timer_;

    /// general buffer for reading etc.
    std::array<char, 2048> buffer_;

    /// set this to debug network problems
    bool logging = false;

    /// general strings used for responses
    std::string header_;
    std::string url_;
    std::string response_;
    std::string data_;

    /// buffers to wrap the strings.
    std::vector<boost::asio::const_buffer> buffers_;

    /// the game we are playing on this connection.
    game *game_;
  };
}

