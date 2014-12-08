#ifndef BRICKS_NET_POSIX_TCP_SERVER_H
#define BRICKS_NET_POSIX_TCP_SERVER_H

#include "exceptions.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <unistd.h>

namespace bricks {
namespace net {

const size_t kMaxQueuedConnections = 1024;

class GenericConnection {
 public:
  inline explicit GenericConnection(const int fd) : fd_(fd) {
  }

  inline GenericConnection(GenericConnection&& rhs) : fd_(-1) {
    std::swap(fd_, rhs.fd_);
  }

  inline ~GenericConnection() {
    if (fd_ != -1) {
      ::close(fd_);
    }
  }

  template <typename T>
  inline size_t BlockingRead(T* buffer, size_t max_length) const {
    uint8_t* raw_buffer = reinterpret_cast<uint8_t*>(buffer);
    uint8_t* raw_ptr = raw_buffer;
    const size_t max_length_in_bytes = max_length * sizeof(T);
    do {
      const ssize_t read_length_or_error = ::read(fd_, raw_ptr, max_length_in_bytes - (raw_ptr - raw_buffer));
      if (read_length_or_error < 0) {
        throw SocketReadException();
      } else if (read_length_or_error == 0) {
        // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
        // and StackOverflow is that a return value of zero from read() from a socket indicates
        // that the socket has been closed by the peer.
        // For this implementation, throw an exception if some record was read only partially.
        if ((raw_ptr - raw_buffer) % sizeof(T)) {
          throw SocketReadMultibyteRecordEndedPrematurelyException();
        }
        break;
      } else {
        raw_ptr += read_length_or_error;
      }
    } while ((raw_ptr - raw_buffer) % sizeof(T));
    return (raw_ptr - raw_buffer) / sizeof(T);
  }

  inline void BlockingWrite(const void* buffer, size_t write_length) {
    assert(buffer);
    const ssize_t result = ::write(fd_, buffer, write_length);
    if (result < 0) {
      throw SocketWriteException();
    } else if (static_cast<size_t>(result) != write_length) {
      throw SocketCouldNotWriteEverythingException();
    }
  }

  inline void BlockingWrite(const char* s) {
    assert(s);
    BlockingWrite(s, strlen(s));
  }

  template <typename T>
  inline void BlockingWrite(const T begin, const T end) {
    BlockingWrite(&(*begin), (end - begin) * sizeof(typename T::value_type));
  }

  // Specialization for STL containers to allow calling BlockingWrite() on std::string, std::vector, etc.
  // The `std::enable_if<>` clause is required otherwise invoking `BlockingWrite(char[N])` does not compile.
  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type)>::type BlockingWrite(const T& container) {
    BlockingWrite(container.begin(), container.end());
  }

 private:
  int fd_ = -1;  // Non-const for move constructor.

  GenericConnection(const GenericConnection&) = delete;
  void operator=(const GenericConnection&) = delete;
  void operator=(GenericConnection&&) = delete;
};

class Connection final : public GenericConnection {
 public:
  inline Connection(GenericConnection&& c) : GenericConnection(std::move(c)) {
  }

  inline Connection(Connection&& c) : GenericConnection(std::move(c)) {
  }
};

class Socket final {
 public:
  inline explicit Socket(int port, int max_connections = kMaxQueuedConnections)
      : socket_(::socket(AF_INET, SOCK_STREAM, 0)) {
    if (socket_ < 0) {
      throw SocketCreateException();
    }

    int just_one = 1;
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &just_one, sizeof(int))) {
      throw SocketCreateException();
    }

    sockaddr_in addr_server{};
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = ::htons(port);

    if (::bind(socket_, (sockaddr*)&addr_server, sizeof(addr_server)) == -1) {
      ::close(socket_);
      throw SocketBindException();
    }

    if (::listen(socket_, max_connections)) {
      ::close(socket_);
      throw SocketListenException();
    }
  }

  inline ~Socket() {
    ::close(socket_);
  }

  inline GenericConnection Accept() const {
    sockaddr_in addr_client{};
    socklen_t addr_client_length = sizeof(sockaddr_in);
    const int fd = ::accept(socket_, (struct sockaddr*)&addr_client, &addr_client_length);
    if (fd == -1) {
      throw SocketAcceptException();
    }
    return GenericConnection(fd);
  }

 private:
  const int socket_;

  Socket(const Socket&) = delete;
  Socket(Socket&&) = delete;
  void operator=(const Socket&) = delete;
  void operator=(Socket&&) = delete;
};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_POSIX_TCP_SERVER_H
