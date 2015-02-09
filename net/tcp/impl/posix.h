/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

// TODO(dkorolev): Add Mac support and find out the right name for this header file.

#ifndef BRICKS_NET_TCP_IMPL_POSIX_H
#define BRICKS_NET_TCP_IMPL_POSIX_H

#include "../../exceptions.h"

#include "../../../util/singleton.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifndef BRICKS_WINDOWS
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#else
#include <WS2tcpip.h>
#include <corecrt_io.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

namespace bricks {
namespace net {

const size_t kMaxServerQueuedConnections = 1024;
const bool kDisableNagleAlgorithmByDefault = false;
const size_t kReadTillEOFInitialBufferSize = 128;

#define kReadTillEOFBufferGrowthK 1.95
// const double kReadTillEOFBufferGrowthK = 1.95;

// D.K.: I have replaced `const double` by `#define`, since clang++
// has been giving the following warning when testing headers for integrity.
//
// >> The symbol is used, but only within a templated method that is not enabled.
// >> /home/dima/github/dkorolev/Bricks/net/tcp/impl/.noshit/headers/posix.h.clang++.cc:26:14: warning: variable
// >>       'kReadTillEOFBufferGrowthK' is not needed and will not be emitted [-Wunneeded-internal-declaration]
// >> const double kReadTillEOFBufferGrowthK = 1.95;
// >>              ^
// >> 1 warning generated.
//
// Interestingly, kReadTillEOFInitialBufferSize follows the same story, but does not trigger a warning.
// Looks like a glitch in clang++ in either direction (warn on both or on none for consistency reasons),
// but I didn't investigate further -- D.K.

struct SocketSystemInitializer {
#ifdef BRICKS_WINDOWS
  struct OneTimeInitializer {
    OneTimeInitializer() {
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        BRICKS_THROW(SocketWSAStartupException());
      }
    }
  };
  SocketSystemInitializer() { Singleton<OneTimeInitializer>(); }
#endif
};

class SocketHandle : private SocketSystemInitializer {
 public:
  // Two ways to construct SocketHandle: via NewHandle() or FromHandle(int handle).
  struct NewHandle final {};
  struct FromHandle final {
    int handle;
    FromHandle(int handle) : handle(handle) {}
  };

  inline SocketHandle(NewHandle) : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      BRICKS_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
  }

  inline SocketHandle(FromHandle from) : socket_(from.handle) {
    if (socket_ < 0) {
      BRICKS_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
  }

  inline ~SocketHandle() {
    if (socket_ != -1) {
#ifndef BRICKS_WINDOWS
      ::close(socket_);
#else
      ::closesocket(socket_);
#endif
    }
  }

  inline explicit SocketHandle(SocketHandle&& rhs) : socket_(-1) { std::swap(socket_, rhs.socket_); }

  // SocketHandle does not expose copy constructor and assignment operator. It should only be moved.

 private:
#ifndef BRICKS_WINDOWS
  int socket_;
#else
  mutable int socket_;  // Need to support taking the handle away from a non-move constructor.
#endif

 public:
  // The `ReadOnlyValidSocketAccessor socket` members provide simple read-only access to `socket_` via `socket`.
  // It is kept here to aggressively restrict access to `socket_` from the outside,
  // since debugging move-constructed socket handles has proven to be nontrivial -- D.K.
  class ReadOnlyValidSocketAccessor final {
   public:
    explicit ReadOnlyValidSocketAccessor(const int& ref) : ref_(ref) {}
    inline operator int() {
      if (!ref_) {
        BRICKS_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
      }
      if (ref_ == -1) {
        BRICKS_THROW(AttemptedToUseMovedAwayConnection());
      }
      return ref_;
    }

   private:
    ReadOnlyValidSocketAccessor() = delete;
    const int& ref_;
  };
  ReadOnlyValidSocketAccessor socket = ReadOnlyValidSocketAccessor(socket_);

 private:
  SocketHandle() = delete;
  void operator=(const SocketHandle&) = delete;
  void operator=(SocketHandle&&) = delete;

#ifndef BRICKS_WINDOWS
  SocketHandle(const SocketHandle&) = delete;
#else
 public:
  SocketHandle(const SocketHandle& rhs) : socket_(-1) { std::swap(socket_, rhs.socket_); }

 private:
#endif
};

class Connection : public SocketHandle {
 public:
  inline Connection(SocketHandle&& rhs) : SocketHandle(std::move(rhs)) {}

  inline Connection(Connection&& rhs) : SocketHandle(std::move(rhs)) {}

  // Closes the outbound side of the socket and notifies the other party that no more data will be sent.
  inline Connection& SendEOF() {
    ::shutdown(socket,
#ifndef BRICKS_WINDOWS
               SHUT_WR
#else
               SD_SEND
#endif
               );
    return *this;
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
    bool alive = true;
    do {
#ifndef BRICKS_WINDOWS
      const ssize_t retval = ::recv(socket, raw_ptr, max_length_in_bytes - (raw_ptr - raw_buffer), 0);
#else
      const int retval =
          ::recv(socket, reinterpret_cast<char*>(raw_ptr), max_length_in_bytes - (raw_ptr - raw_buffer), 0);
#endif
      if (retval < 0) {
        // TODO(dkorolev): Unit-test this logic.
        // I could not find a simple way to reproduce it for the test. -- D.K.
        // LCOV_EXCL_START
        if (errno == EAGAIN) {
          continue;
        } else if (errno == ECONNRESET) {
          // Allow one "Connection reset by peer" error to happen.
          // Tested that this makes load test pass 1000/1000, while w/o this
          // the test fails on some iteration with ECONNRESET on Ubuntu 12.04 -- D.K.
          if (alive) {
            alive = false;
            continue;
          } else {
            if ((raw_ptr - raw_buffer) % sizeof(T)) {
              BRICKS_THROW(SocketReadMultibyteRecordEndedPrematurelyException());
            } else {
              BRICKS_THROW(ConnectionResetByPeer());
            }
          }
        } else {
          if ((raw_ptr - raw_buffer) % sizeof(T)) {
            BRICKS_THROW(SocketReadMultibyteRecordEndedPrematurelyException());
          } else {
            BRICKS_THROW(SocketReadException());
          }
        }
        // LCOV_EXCL_STOP
      } else {
        alive = true;
        if (retval == 0) {
          // This is worth re-checking, but as for 2014/12/06 the concensus of reading through man
          // and StackOverflow is that a return value of zero from read() from a socket indicates
          // that the socket has been closed by the peer.
          // For this implementation, throw an exception if some record was read only partially.
          if ((raw_ptr - raw_buffer) % sizeof(T)) {
            BRICKS_THROW(SocketReadMultibyteRecordEndedPrematurelyException());
          }
          break;
        } else {
          raw_ptr += retval;
        }
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

  inline Connection& BlockingWrite(const void* buffer, size_t write_length) {
    assert(buffer);
#ifndef BRICKS_WINDOWS
    const int result = static_cast<int>(::send(socket, buffer, write_length, MSG_NOSIGNAL));
#else
    // No `MSG_NOSIGNAL` and extra cast for Visual Studio.
    // (As I understand, Windows sockets would not result in pipe-related issues. -- D.K.)
    const int result = static_cast<int>(::send(socket, static_cast<const char*>(buffer), write_length, 0));
#endif
    if (result < 0) {
      BRICKS_THROW(SocketWriteException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    } else if (static_cast<size_t>(result) != write_length) {
      BRICKS_THROW(SocketCouldNotWriteEverythingException());  // This one is tested though.
    }
    return *this;
  }

  inline Connection& BlockingWrite(const char* s) {
    assert(s);
    return BlockingWrite(s, strlen(s));
  }

  template <typename T>
  inline Connection& BlockingWrite(const T begin, const T end) {
    return BlockingWrite(&(*begin), (end - begin) * sizeof(typename T::value_type));
  }

  // Specialization for STL containers to allow calling BlockingWrite() on std::string, std::vector, etc.
  // The `std::enable_if<>` clause is required, otherwise `BlockingWrite(char[N])` becomes ambiguous.
  template <typename T>
  inline typename std::enable_if<sizeof(typename T::value_type) != 0>::type BlockingWrite(const T& container) {
    BlockingWrite(container.begin(), container.end());
  }

 private:
  Connection() = delete;
  Connection(const Connection&) = delete;
  void operator=(const Connection&) = delete;
  void operator=(Connection&& rhs) = delete;
};

class Socket final : public SocketHandle {
 public:
  inline explicit Socket(const int port,
                         const int max_connections = kMaxServerQueuedConnections,
                         const bool disable_nagle_algorithm = kDisableNagleAlgorithmByDefault)
      : SocketHandle(SocketHandle::NewHandle()) {
    int just_one = 1;
    // LCOV_EXCL_START
    if (disable_nagle_algorithm) {
#ifndef BRICKS_WINDOWS
      if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(int)))
#else
      if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&just_one), sizeof(int)))
#endif
      {
        BRICKS_THROW(SocketCreateException());
      }
    }
    // LCOV_EXCL_STOP
    if (::setsockopt(socket,
                     SOL_SOCKET,
                     SO_REUSEADDR,
#ifndef BRICKS_WINDOWS
                     &just_one,
#else
                     reinterpret_cast<const char*>(&just_one),
#endif
                     sizeof(int))) {
      BRICKS_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

    sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    if (::bind(socket, (sockaddr*)&addr_server, sizeof(addr_server)) == -1) {
      BRICKS_THROW(SocketBindException());
    }

    if (::listen(socket, max_connections)) {
      BRICKS_THROW(SocketListenException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
  }

  Socket(Socket&&) = default;

  inline Connection Accept() {
    sockaddr_in addr_client;
    memset(&addr_client, 0, sizeof(addr_client));
    // TODO(dkorolev): Type socklen_t ?
    auto addr_client_length = sizeof(sockaddr_in);
    const int fd = ::accept(socket,
                            reinterpret_cast<struct sockaddr*>(&addr_client),
#ifndef BRICKS_WINDOWS
                            reinterpret_cast<socklen_t*>(&addr_client_length)
#else
                            reinterpret_cast<int*>(&addr_client_length)
#endif
                            );
    if (fd == -1) {
      BRICKS_THROW(SocketAcceptException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
    return Connection(SocketHandle::FromHandle(fd));
  }

  // Note: The copy constructor is left public and default.
  // On Windows builds, it's implemented as a clone of a move constructor for the base class.
  // This turned out to be required since the `std::thread t([](Socket s) { ... }, Socket(port))` construct
  // works fine in g++/clang++ but does not compile in Visual Studio
  // I'll have to investigate it further one day -- D.K.
  Socket(const Socket& rhs) = default;

 private:
  Socket() = delete;
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
      if (retval || !servinfo) {
        if (retval) {
          // TODO(dkorolev): LOG(somewhere, strings::Printf("Error in getaddrinfo: %s\n",
          // gai_strerror(retval)));
        }
        BRICKS_THROW(SocketResolveAddressException());
      }
      struct sockaddr* p_addr = servinfo->ai_addr;
      // TODO(dkorolev): Use a random address, not the first one. Ref. iteration:
      // for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      //   p->ai_addr;
      // }
      const int retval2 = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval2) {
        BRICKS_THROW(SocketConnectException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
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
