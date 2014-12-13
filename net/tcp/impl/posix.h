// TODO(dkorolev): Add Mac support and find out the right name for this header file.

#ifndef BRICKS_NET_TCP_IMPL_POSIX_H
#define BRICKS_NET_TCP_IMPL_POSIX_H

#include "../../exceptions.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace bricks {
namespace net {

const size_t kMaxServerQueuedConnections = 1024;
const bool kDisableNagleAlgorithmByDefault = false;
const size_t kReadTillEOFInitialBufferSize = 128;
const double kReadTillEOFBufferGrowthK = 1.95;

class SocketHandle {
 public:
  // Two ways to construct SocketHandle: via NewHandle() or FromHandle(int handle).
  struct NewHandle final {};
  struct FromHandle final {
    int handle;
    FromHandle(int handle) : handle(handle) {
    }
  };

  inline SocketHandle(NewHandle) : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      throw SocketCreateException();
    }
  }

  inline SocketHandle(FromHandle from) : socket_(from.handle) {
    if (socket_ < 0) {
      throw InvalidSocketException();
    }
  }

  inline ~SocketHandle() {
    if (socket_ != -1) {
      ::close(socket_);
    }
  }

  inline explicit SocketHandle(SocketHandle&& rhs) : socket_(-1) {
    std::swap(socket_, rhs.socket_);
  }

  void operator=(SocketHandle&& rhs) {
    socket_ = -1;
    std::swap(socket_, rhs.socket_);
  }

 private:
  int socket_;

 public:
  // The `ReadOnlyIntFieldAccessor socket` members provide simple read-only access to `socket_` via `socket`.
  class ReadOnlyIntFieldAccessor final {
   public:
    explicit ReadOnlyIntFieldAccessor(const int& ref) : ref_(ref) {
    }
    inline operator int() {
      if (!ref_) {
        throw InvalidSocketException();
      }
      return ref_;
    }

   private:
    ReadOnlyIntFieldAccessor() = delete;
    const int& ref_;
  };
  ReadOnlyIntFieldAccessor socket = ReadOnlyIntFieldAccessor(socket_);

 private:
  SocketHandle() = delete;
  SocketHandle(const SocketHandle&) = delete;
  void operator=(const SocketHandle&) = delete;
};

class Connection : public SocketHandle {
 public:
  inline Connection(SocketHandle&& rhs) : SocketHandle(std::move(rhs)) {
  }

  inline Connection(Connection&& rhs) : SocketHandle(std::move(rhs)) {
  }

  inline void operator=(Connection&& rhs) {
    SocketHandle::operator=(std::move(rhs));
  }

  // Closes the outbound side of the socket and notifies the other party that no more data will be sent.
  inline void SendEOF() {
    ::shutdown(socket, SHUT_WR);
  }

  // By default, BlockingRead() will return as soon as some data has been read,
  // with the exception being multibyte records (sizeof(T) > 1), where it will keep reading
  // until the boundary of the records, or max_length of them, has been read.
  // Alternatively, the 3rd parameter can be explicitly set to BlockingReadPolicy::FillFullBuffer,
  // which will cause BlockingRead() to keep reading more data until all `max_elements` are read or EOF is hit.
  enum BlockingReadPolicy { ReturnASAP = false, FillFullBuffer = true };
  template <typename T>
  inline size_t BlockingRead(T* buffer,
                             size_t max_length,
                             BlockingReadPolicy policy = BlockingReadPolicy::ReturnASAP) {
    uint8_t* raw_buffer = reinterpret_cast<uint8_t*>(buffer);
    uint8_t* raw_ptr = raw_buffer;
    const size_t max_length_in_bytes = max_length * sizeof(T);
    do {
      const ssize_t retval = ::read(socket, raw_ptr, max_length_in_bytes - (raw_ptr - raw_buffer));
      if (retval < 0) {
        throw SocketReadException();
      } else if (retval == 0) {
        // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
        // and StackOverflow is that a return value of zero from read() from a socket indicates
        // that the socket has been closed by the peer.
        // For this implementation, throw an exception if some record was read only partially.
        if ((raw_ptr - raw_buffer) % sizeof(T)) {
          throw SocketReadMultibyteRecordEndedPrematurelyException();
        }
        break;
      } else {
        raw_ptr += retval;
      }
    } while (policy == BlockingReadPolicy::FillFullBuffer || ((raw_ptr - raw_buffer) % sizeof(T)) > 0);
    return (raw_ptr - raw_buffer) / sizeof(T);
  }

  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type) != 0, const T&>::type BlockingReadUntilEOF(
      T& container,
      const size_t initial_size = kReadTillEOFInitialBufferSize,
      const double growth_k = kReadTillEOFBufferGrowthK) {
    container.resize(initial_size);
    size_t offset = 0;
    size_t desired;
    size_t actual;
    while (desired = container.size() - offset,
           actual = BlockingRead(&container[offset], desired, BlockingReadPolicy::FillFullBuffer),
           actual == desired) {
      offset += desired;
      container.resize(static_cast<size_t>(container.size() * growth_k));
    }
    container.resize(offset + actual);
    return container;
  }

  template <typename T = std::string>
  inline typename std::enable_if<sizeof(typename T::value_type) != 0, T>::type BlockingReadUntilEOF() {
    T container;
    BlockingReadUntilEOF(container);
    return container;
  }

  inline void BlockingWrite(const void* buffer, size_t write_length) {
    assert(buffer);
    const ssize_t result = ::write(socket, buffer, write_length);
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
  Connection() = delete;
  Connection(const Connection&) = delete;
  void operator=(const Connection&) = delete;
};

class Socket final : public SocketHandle {
 public:
  inline explicit Socket(const int port,
                         const int max_connections = kMaxServerQueuedConnections,
                         const bool disable_nagle_algorithm = kDisableNagleAlgorithmByDefault)
      : SocketHandle(SocketHandle::NewHandle()) {
    int just_one = 1;
    if (disable_nagle_algorithm) {
      if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(int))) {
        throw SocketCreateException();
      }
    }
    if (::setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &just_one, sizeof(int))) {
      throw SocketCreateException();
    }

    sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));  // Demote the warning.
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    if (::bind(socket, (sockaddr*)&addr_server, sizeof(addr_server)) == -1) {
      throw SocketBindException();
    }

    if (::listen(socket, max_connections)) {
      throw SocketListenException();
    }
  }

  Socket(Socket&&) = default;

  inline Connection Accept() {
    sockaddr_in addr_client;
    memset(&addr_client, 0, sizeof(addr_client));
    socklen_t addr_client_length = sizeof(sockaddr_in);
    const int fd = ::accept(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length);
    if (fd == -1) {
      throw SocketAcceptException();
    }
    return Connection(SocketHandle::FromHandle(fd));
  }

 private:
  Socket() = delete;
  Socket(const Socket&) = delete;
  void operator=(const Socket&) = delete;
  void operator=(Socket&&) = delete;
};

// POSIX allows numeric ports, as well as strings like "http".
template <typename T>
inline Connection ClientSocket(const std::string& host, T port_or_serv) {
  class ClientSocket final : public SocketHandle {
   public:
    inline explicit ClientSocket(const std::string& host, const std::string& serv)
        : SocketHandle(SocketHandle::NewHandle()) {
      struct addrinfo hints;
      memset(&hints, 0, sizeof(hints));
      struct addrinfo* servinfo;
      hints.ai_family = AF_INET;
      hints.ai_socktype = SOCK_STREAM;
      hints.ai_protocol = IPPROTO_TCP;
      const int retval = ::getaddrinfo(host.c_str(), serv.c_str(), &hints, &servinfo);
      if (retval) {
        // TODO(dkorolev): LOG(somewhere, strings::Printf("Error in getaddrinfo: %s\n", gai_strerror(retval)));
        throw SocketResolveAddressException();
      }
      if (!servinfo) {
        throw SocketResolveAddressException();
      }
      struct sockaddr* p_addr = servinfo->ai_addr;
      // TODO(dkorolev): Use a random address, not the first one. Ref. iteration:
      // for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      //   p->ai_addr;
      // }
      const int retval2 = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval2) {
        throw SocketConnectException();
      }
      // TODO(dkorolev): Free this one, make use of Alex's ScopeGuard.
      ::freeaddrinfo(servinfo);
    }
  };

  return Connection(ClientSocket(host, std::to_string(port_or_serv)));
}

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_TCP_IMPL_POSIX_H
