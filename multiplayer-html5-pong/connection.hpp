namespace game_server {

class connection {
public:
  connection(io_service &io, socket &sock, game *_game) :
    io_(io),
    socket_(std::move(sock)),
    timer_(io),
    game_(_game)
  {
    std::cout << "accepted " << socket_.native_handle() << "\n";
    do_tick();
  }

  bool is_open() {
    return socket_.is_open();
  }
private:
  // parse all or part of an html request header
  // return false if it fails.
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
      if (logging) std::cout << std::string(header_.begin(), p);
      header_.assign(p, e);
      return true;
    }
    return false;
  }

  // initiate and handle a read request.
  // the lambda will live on after do_read has returned.
  void do_read() {
    if (logging) std::cout << "do_read " << socket_.native_handle() << "\n";
    socket_.async_read_some(boost::asio::buffer(buffer_),
      [this](boost::system::error_code ec, std::size_t bytes_transferred) {
        if (logging) std::cout << "read " << socket_.native_handle() << " err=" << ec << "\n";
        if (!ec) {
          if (logging) std::cout << socket_.native_handle() << " read " << bytes_transferred << "\n";
          header_.append(buffer_.data(), buffer_.data() + bytes_transferred);
          if (parse_header()) {
            if (!send_response()) {
              send_http("HTTP/1.1 404 Not Found\r\n", "text/html", "<html><body>Not found</body></html>");
            }
            do_write();
          }
        } else {
          if (socket_.is_open()) {
            std::cout << "close err=" << ec << " " << socket_.native_handle() << "\n";
            socket_.close();
          }
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
        do_read();
        do_tick();
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
    response_.append("\r\n");
    response_.append("Content-Type: ");
    response_.append(mime);
    response_.append("\r\n");
    response_.append("\r\n");
    response_.append(str);
    buffers_.clear();
    buffers_.emplace_back(boost::asio::buffer(response_));
  }

  bool send_response() {
    if (logging) std::cout << "url=" << url_ << "\n";

    if (url_ == "/data") {
      if (logging) std::cout << "back!\n";

      // run a frame of the game and write
      game_->do_frame();

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

  std::array<char, 2048> buffer_;

  int frame = 0;

  bool logging = false;

  socket socket_;
  boost::asio::deadline_timer timer_;

  std::string header_;
  std::string url_;
  std::string response_;
  std::string data_;
  std::vector<boost::asio::const_buffer> buffers_;

  game *game_;
};


}

