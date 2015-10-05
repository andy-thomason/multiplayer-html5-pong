
namespace game_server {

  using boost::asio::ip::tcp;

  class server {
  public:
    server(boost::asio::io_service &io) :
      io_(io),
      acceptor_(io, tcp::endpoint(tcp::v4(), 8000)),
      socket_(io)
    {
      // todo: add lobbying
      games.push_back(new game());
      do_accept();
    }

    void do_accept() {
      acceptor_.async_accept(socket_,
        [this](boost::system::error_code ec)
        {
          // Check whether the server was stopped by a signal before this
          // completion handler had a chance to run.
          if (acceptor_.is_open() && !ec) {
            // remove dead connections
            size_t i = 0, j = 0;
            for (; i != connections.size(); ++i) {
              if (connections[i]->is_open()) {
                connections[j++] = connections[i];
              } else {
                // todo: fix this bug where asio dies when we delete.
                //delete connections[i];
              }
            }
            connections.resize(j);

            // add new connection
            connections.push_back(new connection(io_, std::move(socket_), games[0]));
          }
          do_accept();
        }
      );
    }

  private:
    // we can have many people playing the game
    std::vector<connection *> connections;

    // we can have many games.
    std::vector<game *> games;

    /// The io used to perform asynchronous operations.
    io_service &io_;

    /// Acceptor used to listen for incoming connections.
    boost::asio::ip::tcp::acceptor acceptor_;

    /// The next socket to be accepted.
    socket socket_;
  };

}
