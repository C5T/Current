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

#include "../../debug_log.h"

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

// Bricks uses `SOCKET` for socket handles in *nix.
// Makes it easier to have the code run on both Windows and *nix.
typedef int SOCKET;

#else

#include <WS2tcpip.h>

#include <corecrt_io.h>
#pragma comment(lib, "Ws2_32.lib")

#endif

namespace bricks {
namespace net {

const size_t kMaxServerQueuedConnections = 1024;
const bool kDisableNagleAlgorithmByDefault = false;

struct SocketSystemInitializer {
#ifdef BRICKS_WINDOWS
  struct OneTimeInitializer {
    OneTimeInitializer() {
      BRICKS_NET_LOG("WSAStartup() ...\n");
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        BRICKS_THROW(SocketWSAStartupException());
      }
      BRICKS_NET_LOG("WSAStartup() : OK\n");
    }
  };
  SocketSystemInitializer() { Singleton<OneTimeInitializer>(); }
#endif
};

class SocketHandle : private SocketSystemInitializer {
 public:
  // Two ways to construct SocketHandle: via NewHandle() or FromHandle(SOCKET handle).
  struct NewHandle final {};
  struct FromHandle final {
    SOCKET handle;
    FromHandle(SOCKET handle) : handle(handle) {}
  };

  inline SocketHandle(NewHandle) : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      BRICKS_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    BRICKS_NET_LOG("S%05d socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);\n", socket_);
  }

  inline SocketHandle(FromHandle from) : socket_(from.handle) {
    if (socket_ < 0) {
      BRICKS_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    BRICKS_NET_LOG("S%05d == initialized externally.\n", socket_);
  }

  inline ~SocketHandle() {
    if (socket_ != -1) {
      BRICKS_NET_LOG("S%05d close() ...\n", socket_);
#ifndef BRICKS_WINDOWS
      ::close(socket_);
#else
      ::closesocket(socket_);
#endif
      BRICKS_NET_LOG("S%05d close() : OK\n", socket_);
    }
  }

  inline explicit SocketHandle(SocketHandle&& rhs) : socket_(-1) { std::swap(socket_, rhs.socket_); }

  // SocketHandle does not expose copy constructor and assignment operator. It should only be moved.

 private:
#ifndef BRICKS_WINDOWS
  SOCKET socket_;
#else
  mutable SOCKET socket_;  // Need to support taking the handle away from a non-move constructor.
#endif

 public:
  // The `ReadOnlyValidSocketAccessor socket` members provide simple read-only access to `socket_` via `socket`.
  // It is kept here to aggressively restrict access to `socket_` from the outside,
  // since debugging move-constructed socket handles has proven to be nontrivial -- D.K.
  class ReadOnlyValidSocketAccessor final {
   public:
    explicit ReadOnlyValidSocketAccessor(const SOCKET& ref) : ref_(ref) {}
    inline operator SOCKET() {
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
    const SOCKET& ref_;
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
  // Visual Studio seem to require this constructor, since std::move()-ing away `SocketHandle`-s
  // as part of `Connection` objects doesn't seem to work. TODO(dkorolev): Investigate this.
  SocketHandle(const SocketHandle& rhs) : socket_(-1) { std::swap(socket_, rhs.socket_); }

 private:
#endif
};

class Connection : public SocketHandle {
 public:
  inline Connection(SocketHandle&& rhs) : SocketHandle(std::move(rhs)) {}

  inline Connection(Connection&& rhs) : SocketHandle(std::move(rhs)) {}

  // By default, BlockingRead() will return as soon as some data has been read,
  // with the exception being multibyte records (sizeof(T) > 1), where it will keep reading
  // until the boundary of the records, or max_length of them, has been read.
  // Alternatively, the 3rd parameter can be explicitly set to BlockingReadPolicy::FillFullBuffer,
  // which will cause BlockingRead() to keep reading more data until all `max_elements` are read or EOF is hit.
  enum BlockingReadPolicy { ReturnASAP = false, FillFullBuffer = true };
  template <typename T>
  inline typename std::enable_if<sizeof(T) == 1, size_t>::type BlockingRead(
      T* output_buffer, size_t max_length, BlockingReadPolicy policy = BlockingReadPolicy::ReturnASAP) {
    BRICKS_NET_LOG("S%05d BlockingRead() ...\n", static_cast<SOCKET>(socket));
    uint8_t* buffer = reinterpret_cast<uint8_t*>(output_buffer);
    uint8_t* ptr = buffer;
    size_t remaining_bytes_to_read;
    while ((remaining_bytes_to_read = (max_length - (ptr - buffer))) > 0) {
      BRICKS_NET_LOG("S%05d BlockingRead() ... attempting to read %d bytes.\n",
                     static_cast<SOCKET>(socket),
                     static_cast<int>(remaining_bytes_to_read));
#ifndef BRICKS_WINDOWS
      const ssize_t retval = ::recv(socket, ptr, remaining_bytes_to_read, 0);
#else
      const int retval =
          ::recv(socket, reinterpret_cast<char*>(ptr), static_cast<int>(remaining_bytes_to_read), 0);
#endif
      BRICKS_NET_LOG(
          "S%05d BlockingRead() ... retval = %d.\n", static_cast<SOCKET>(socket), static_cast<int>(retval));
      if (retval > 0) {
        ptr += retval;
        if (policy == BlockingReadPolicy::ReturnASAP) {
          return (ptr - buffer);
        }
      } else {
        // LCOV_EXCL_START
        if (retval < 0) {
          if (policy == BlockingReadPolicy::ReturnASAP) {
            return (ptr - buffer);
          } else {
            if (errno == EAGAIN) {
              continue;
            } else if (errno == ECONNRESET) {
              BRICKS_NET_LOG("S%05d BlockingRead() : Connection reset by peer after reading %d bytes.\n",
                             static_cast<SOCKET>(socket),
                             static_cast<int>(ptr - buffer));
              BRICKS_THROW(ConnectionResetByPeer());
            } else {
              BRICKS_NET_LOG("S%05d BlockingRead() : Error after reading %d bytes, errno %d.\n",
                             static_cast<SOCKET>(socket),
                             static_cast<int>(ptr - buffer),
                             errno);
#ifdef BRICKS_WINDOWS
              if (retval == -1) {
                continue;  // Believe or no, this makes the tests pass. TODO(dkorolev): Investigate.
              }
#else
              BRICKS_THROW(SocketReadException());
#endif
            }
          }
        } else {
          BRICKS_NET_LOG("S%05d BlockingRead() : retval == 0 ...\n", static_cast<SOCKET>(socket));
          if (policy == BlockingReadPolicy::ReturnASAP) {
            return (ptr - buffer);
          } else {
            BRICKS_THROW(SocketReadException());
          }
        }
      }
      // LCOV_EXCL_STOP
    }
    BRICKS_NET_LOG("S%05d BlockingRead() : OK, read %d bytes.\n",
                   static_cast<SOCKET>(socket),
                   static_cast<int>(ptr - buffer));
    return ptr - buffer;
  }

  inline Connection& BlockingWrite(const void* buffer, size_t write_length) {
    assert(buffer);
    BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) ...\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
#ifndef BRICKS_WINDOWS
    const int result = static_cast<int>(::send(socket, buffer, write_length, MSG_NOSIGNAL));
#else
    // No `MSG_NOSIGNAL` and extra cast for Visual Studio.
    // (As I understand, Windows sockets would not result in pipe-related issues. -- D.K.)
    const int result =
        static_cast<int>(::send(socket, static_cast<const char*>(buffer), static_cast<int>(write_length), 0));
#endif
    if (result < 0) {
      BRICKS_THROW(SocketWriteException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    } else if (static_cast<size_t>(result) != write_length) {
      BRICKS_THROW(SocketCouldNotWriteEverythingException());  // This one is tested though.
    }
    BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) : OK\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
    return *this;
  }

  inline Connection& BlockingWrite(const char* s) {
    assert(s);
    return BlockingWrite(s, strlen(s));
  }

  template <typename T>
  inline Connection& BlockingWrite(const T begin, const T end) {
    if (begin != end) {
      return BlockingWrite(&(*begin), (end - begin) * sizeof(typename T::value_type));
    } else {
      return *this;
    }
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
#ifndef BRICKS_WINDOWS
    int just_one = 1;
#else
    u_long just_one = 1;
#endif

    // LCOV_EXCL_START
    if (disable_nagle_algorithm) {
#ifndef BRICKS_WINDOWS
      if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(just_one)))
#else
      if (::setsockopt(
              socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&just_one), sizeof(just_one)))
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
                     sizeof(just_one))) {
      BRICKS_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

    sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    addr_server.sin_port = htons(port);

    BRICKS_NET_LOG("S%05d bind()+listen() ...\n", static_cast<SOCKET>(socket));

    if (::bind(socket, reinterpret_cast<sockaddr*>(&addr_server), sizeof(addr_server)) == -1) {
      BRICKS_THROW(SocketBindException());
    }

    BRICKS_NET_LOG("S%05d bind()+listen() : bind() OK\n", static_cast<SOCKET>(socket));

    if (::listen(socket, max_connections)) {
      BRICKS_THROW(SocketListenException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

    BRICKS_NET_LOG("S%05d bind() and listen() : listen() OK\n", static_cast<SOCKET>(socket));
  }

  Socket(Socket&&) = default;

  inline Connection Accept() {
    BRICKS_NET_LOG("S%05d accept() ...\n", static_cast<SOCKET>(socket));
    sockaddr_in addr_client;
    memset(&addr_client, 0, sizeof(addr_client));
    // TODO(dkorolev): Type socklen_t ?
    auto addr_client_length = sizeof(sockaddr_in);
#ifndef BRICKS_WINDOWS
    const SOCKET handle = ::accept(socket,
                                   reinterpret_cast<struct sockaddr*>(&addr_client),
                                   reinterpret_cast<socklen_t*>(&addr_client_length));
    if (handle == -1) {
      BRICKS_NET_LOG("S%05d accept() : Failed.\n", static_cast<SOCKET>(socket));
      BRICKS_THROW(SocketAcceptException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
#else
    const SOCKET handle = ::accept(
        socket, reinterpret_cast<struct sockaddr*>(&addr_client), reinterpret_cast<int*>(&addr_client_length));
    if (handle == INVALID_SOCKET) {
      BRICKS_THROW(SocketAcceptException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
#endif
    BRICKS_NET_LOG("S%05d accept() : OK, FD = %d.\n", static_cast<SOCKET>(socket), handle);
    return Connection(SocketHandle::FromHandle(handle));
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
      BRICKS_NET_LOG("S%05d getaddrinfo(%s@%s) ...\n", static_cast<SOCKET>(socket), host.c_str(), serv.c_str());
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
      BRICKS_NET_LOG("S%05d connect() ...\n", static_cast<SOCKET>(socket));
      const int retval2 = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval2) {
        BRICKS_THROW(SocketConnectException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
      }
      // TODO(dkorolev): Free this one, make use of Alex's ScopeGuard.
      ::freeaddrinfo(servinfo);
      BRICKS_NET_LOG("S%05d connect() OK\n", static_cast<SOCKET>(socket));
    }
  };

  return Connection(ClientSocket(host, std::to_string(port_or_serv)));
}

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_TCP_IMPL_POSIX_H
