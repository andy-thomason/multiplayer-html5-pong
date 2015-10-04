
#define BOOST_ASIO_HAS_MOVE 1
#define BOOST_ASIO_ERROR_CATEGORY_NOEXCEPT noexcept(true)
#define _WIN32_WINNT 0x0501

#include <boost/asio.hpp>
#include <iostream>
#include <fstream>
#include <vector>

#include "game.hpp"
#include "connection.hpp"
#include "server.hpp"

int main() {
  boost::asio::io_service io;

  game_server::server svr(io);
  io.run();
}
