// TODO(dkorolev): Resolve the sleep() hack.
// TODO(dkorolev): Refactoring: merge GenericConnection and Connection, make Socket keep one.
#ifndef BRICKS_NET_POSIX_TCP_SERVER_H
#define BRICKS_NET_POSIX_TCP_SERVER_H

#include "exceptions.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <chrono>  // -- Hack by Dima.

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
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

  // Closes the outbound side of the socket and notifies the other party that no more data will be sent.
  inline void SendEOF() {
    ::fsync(fd_);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Hack by Dima.
    ::shutdown(fd_, SHUT_WR);
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
  inline typename std::enable_if<sizeof(typename T::value_type) != 0>::type BlockingWrite(const T& container) {
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

class SocketHandle {
 public:
  inline explicit SocketHandle() : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      throw SocketCreateException();
    }
  }

  inline ~SocketHandle() {
    if (socket_ != -1) {
      ::close(socket_);
    }
  }

  // Releases socket handle to make it possible for Connection object to outlive the Socket one.
  // Effectively, move-constructs the handle away.
  // TODO(dkorolev): Can't we just move-construct it all the time?
  // Used for client-side sockets, where just Connection is being returned.
  inline int ReleaseSocketHandle() {
    int result = -1;
    std::swap(result, socket_);
    return result;
  }

 protected:
  int socket_;

 private:
  SocketHandle(const SocketHandle&) = delete;
  SocketHandle(SocketHandle&&) = delete;
  void operator=(const SocketHandle&) = delete;
  void operator=(SocketHandle&&) = delete;
};

class Socket final : public SocketHandle {
 public:
  inline explicit Socket(int port, int max_connections = kMaxQueuedConnections) : SocketHandle() {
    int just_one = 1;
    if (::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(int))) {
      throw SocketCreateException();
    }
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &just_one, sizeof(int))) {
      throw SocketCreateException();
    }

    sockaddr_in addr_server{};
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    if (::bind(socket_, (sockaddr*)&addr_server, sizeof(addr_server)) == -1) {
      throw SocketBindException();
    }

    if (::listen(socket_, max_connections)) {
      throw SocketListenException();
    }
  }

  inline GenericConnection Accept() const {
    sockaddr_in addr_client{};
    socklen_t addr_client_length = sizeof(sockaddr_in);
    const int fd = ::accept(socket_, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length);
    if (fd == -1) {
      throw SocketAcceptException();
    }
    return GenericConnection(fd);
  }
};

// POSIX allows numeric ports, as well as strings like "http".
template <typename T>
inline GenericConnection ClientSocket(const std::string& host, T port_or_serv) {
  class ClientSocket final : public SocketHandle {
   public:
    inline explicit ClientSocket(const std::string& host, const std::string& serv) : SocketHandle() {
      struct addrinfo hints {};
      struct addrinfo* servinfo;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP;
      const int retval = ::getaddrinfo(host.c_str(), serv.c_str(), &hints, &servinfo);
      if (retval) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(retval));
        throw SocketResolveAddressException();
      }
      if (!servinfo) {
        throw SocketResolveAddressException();
      }
      struct sockaddr* p_addr = servinfo->ai_addr;
      // TODO(dkorolev): Use a random address, not the first one. Use this loop:
      // for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      //   p->ai_addr;
      // }
      if (::connect(socket_, p_addr, sizeof(*p_addr))) {
        throw SocketConnectException();
      }
      freeaddrinfo(servinfo);
    }
  };

  GenericConnection cc = [&]() {
    ClientSocket client_socket(host, std::to_string(port_or_serv));
    GenericConnection c(client_socket.ReleaseSocketHandle());
    return c;
  }();
  return cc;
}

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_POSIX_TCP_SERVER_H
