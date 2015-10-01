
// If this does not compile, you may have the buggy Windows 8 SDK. Upgrade it!
#ifdef WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment (lib, "Ws2_32.lib")
  typedef unsigned socket_t;
  static auto socket_error = SOCKET_ERROR;
#endif

#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// see https://msdn.microsoft.com/en-gb/library/windows/desktop/ms737593(v=vs.85).aspx


class connection {
public:
  connection(socket_t client_socket, std::ostream &log) : client_socket(client_socket), log(log) {
    log << "create " << client_socket << "\n";
    state = state_t::reading_header;
    url.clear();
    keep_alive = 100;
  }

  connection(connection &&rhs) : log(rhs.log) {
    client_socket = rhs.client_socket;
    rhs.client_socket = socket_error;
  }

  connection & operator=(connection &&rhs) {
    client_socket = rhs.client_socket;
    rhs.client_socket = socket_error;
    return *this;
  }

  void close_connection() {
    log << "close " << client_socket << "\n";
    closesocket(client_socket);
    client_socket = socket_error;
    state = state_t::reading_header;
  }

  void poll_read(std::ostream &log, int t) {
    char tmp[10000];
    log << "recv...\n";
    int r = recv(client_socket, tmp, sizeof(tmp), 0);

    if (r > 0) {
      log << "recv " << client_socket << " " << r << "\n";
      keep_alive = 100;
      if (true || state == state_t::reading_header) {
        tmp[r] = 0;
        log.write(tmp, r);
        if (parse_http_request(tmp)) {
          if (url == "/") {
            write_header();
            send("<html><body>\n");
            state = state_t::writing_resp;
          } else {
            write_not_found();
            close_connection();
          }
        }
      } else {
        log << "unexpected read\n";
      }
    } else if (r == 0) {
       close_connection();
    }
  }

  void poll_write(std::ostream &log, int t) {
  }

  void poll_timeout(std::ostream &log, int t) {
    keep_alive = 100;
    if (state == state_t::writing_resp) {
      send("hello\n");
    }
  }

  void poll_err(std::ostream &log, int t) {
    int error_code;
    int error_code_size = sizeof(error_code);
    getsockopt(client_socket, SOL_SOCKET, SO_ERROR, (char*)&error_code, &error_code_size);
    if (error_code) log << "error " << client_socket << " e=" << error_code << "\n";
    if (--keep_alive == 0) {
      close_connection();
    }
  }

  void send(const std::string &str) {
    int res = ::send(client_socket, str.c_str(), (int)str.size(), 0);
    //log << "send " << res << "\n";
    log.write(str.c_str(), (int)str.size());
    //log << "err=" << WSAGetLastError() << "\n";
  }

  void write_not_found() {
    std::string header;
    header.append("HTTP ");
    header.append(url);
    header.append(" 1.1 404 Not Found\r\n");
    header.append("\r\n");
    send(header);
  }

  void write_http(const std::string &str) {
    std::string header;
    header.append("HTTP / 1.1 200 OK\r\n");
    header.append("Content - Type : text / html; charset = UTF - 8\r\n");
    header.append("Content - Length: ");
    char tmp[32]; sprintf(tmp, "%d", (int)str.size());
    header.append(tmp);
    header.append("\r\n");
    header.append("\r\n");
    header.append(str);
    send(header);
  }

  void write_header() {
    std::string header;
    header.append("HTTP / 1.1 200 OK\r\n");
    header.append("Content - Type : text / html; charset = UTF - 8\r\n");
    header.append("\r\n");
    send(header);
  }

  bool parse_http_request(const char *p) {
    url.clear();
    bool blank = false;
    while (*p) {
      const char *b = p;
      while (*p && *p != '\n') ++p;
      if (*p) ++p;
      if (b[0] == 'G' && b[1] == 'E' && b[2] == 'T') {
        const char *u = b + 3;
        while (*u == ' ') ++u;
        const char *u2 = u;
        while (*u2 && *u2 != ' ') ++u2;
        url.assign(u, u2);
        log << "url = " << url << "\n";
      } else if (b[0] == '\r' && b[1] == '\n' && b[2] == 0) {
        blank = true;
        break;
      }
    }
    return blank;
  }

  bool ready_to_write() {
    return false;
  }

  ~connection() {
    if (!is_dead()) {
      close_connection();
    }
  }

  bool is_dead() {
    return client_socket == socket_error;
  }

  socket_t socket() { return client_socket; }
private:
  socket_t client_socket;
  std::ostream &log;
  int keep_alive;

  enum class state_t {
    reading_header,
    writing_resp,
  };
  state_t state;
  std::string url;
};

class server {
public:
  server(std::ostream &log) : log(log) {
    log << "hello\n";
    listen_socket = 0;
    #ifdef WIN32
      WSADATA wsaData;
      WSAStartup(MAKEWORD(2, 2), &wsaData);
    #endif

    struct addrinfo hints = { 0 };
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    struct addrinfo *result = nullptr;
    getaddrinfo(nullptr, "8000", &hints, &result);

    listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);

    freeaddrinfo(result);

    listen(listen_socket, 4);
  }

  void serve_forever() {
    fd_set rfds = {};
    fd_set wfds = {};
    fd_set efds = {};
    timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 1000000;

    int t = 0;
    for (;;) {
      log << ".";
      // select will wait for incomming connections or read data.
      rfds.fd_count = 0;
      rfds.fd_array[rfds.fd_count++] = listen_socket;
      wfds.fd_count = 0;
      efds.fd_count = 0;
      for (auto c : connections) {
        rfds.fd_array[rfds.fd_count++] = c->socket();
        if (c->ready_to_write()) wfds.fd_array[wfds.fd_count++] = c->socket();
        efds.fd_array[efds.fd_count++] = c->socket();
      }

      //log << "select..." << rfds.fd_count << "\n";
      int val = ::select(0, &rfds, &wfds, &efds, &timeout);
      if (val == 0) {
        for (size_t i = 0; i < connections.size(); ++i) {
          auto c = connections[i];
          c->poll_timeout(log, t);
        }
        ++t;
      } else {
        //log << "select " << val << " r=" << rfds.fd_count << " w=" << wfds.fd_count << " e=" << efds.fd_count << "\n";

        for (int i = 0; i != rfds.fd_count; ++i) {
          socket_t sock = rfds.fd_array[i];
          if (sock == listen_socket) {
            // check to see if we have a new connection.
            socket_t client_socket = accept(listen_socket, nullptr, nullptr);
            if (client_socket != socket_error) {
              connections.push_back(new connection(client_socket, log));
            }
          } else {
            for (size_t i = 0; i < connections.size(); ++i) {
              auto c = connections[i];
              if (sock == c->socket()) {
                c->poll_read(log, t);
              }
            }
          }
        }

        for (int i = 0; i != wfds.fd_count; ++i) {
          socket_t sock = wfds.fd_array[i];
          for (size_t i = 0; i < connections.size(); ++i) {
            auto c = connections[i];
            if (sock == c->socket()) {
              c->poll_write(log, t);
            }
          }
        }

        for (int i = 0; i != efds.fd_count; ++i) {
          socket_t sock = efds.fd_array[i];
          for (size_t i = 0; i < connections.size(); ++i) {
            auto c = connections[i];
            if (sock == c->socket()) {
              c->poll_err(log, t);
            }
          }
        }

        // kill dead connections
        for (size_t i = 0; i < connections.size(); ++i) {
          auto c = connections[i];
          if (c->is_dead()) {
            delete c;
            connections.erase(connections.begin() + i);
          }
        }
      }
    }
  }

  ~server() {
    WSACleanup();
  }
private:
  socket_t listen_socket;
  std::vector<connection*> connections;
  std::ostream &log;
};

int main() {
  //std::ofstream log("log.txt");
  server svr(std::cout);

  svr.serve_forever();
}


