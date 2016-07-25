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
#include "../../../template/enable_if.h"

#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#ifndef CURRENT_WINDOWS

#include <errno.h>
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

namespace current {
namespace net {

const size_t kMaxServerQueuedConnections = 1024;
const bool kDisableNagleAlgorithmByDefault = false;

struct SocketSystemInitializer {
#ifdef CURRENT_WINDOWS
  struct OneTimeInitializer {
    OneTimeInitializer() {
      BRICKS_NET_LOG("WSAStartup() ...\n");
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        CURRENT_THROW(SocketWSAStartupException());
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

  inline SocketHandle(NewHandle, const bool disable_nagle_algorithm = kDisableNagleAlgorithmByDefault)
      : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      CURRENT_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    BRICKS_NET_LOG("S%05d socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);\n", socket_);

#ifndef CURRENT_WINDOWS
    int just_one = 1;
#else
    u_long just_one = 1;
#endif

    // LCOV_EXCL_START
    if (disable_nagle_algorithm) {
#ifndef CURRENT_WINDOWS
      if (::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(just_one)))
#else
      if (::setsockopt(
              socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&just_one), sizeof(just_one)))
#endif
      {
        CURRENT_THROW(SocketCreateException());
      }
    }

    // LCOV_EXCL_STOP
    if (::setsockopt(socket_,
                     SOL_SOCKET,
                     SO_REUSEADDR,
#ifndef CURRENT_WINDOWS
                     &just_one,
#else
                     reinterpret_cast<const char*>(&just_one),
#endif
                     sizeof(just_one))) {
      CURRENT_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

#ifdef CURRENT_APPLE
    // Emulate MSG_NOSIGNAL behavior.
    if (::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, static_cast<void*>(&just_one), sizeof(just_one))) {
      CURRENT_THROW(SocketCreateException());
    }
#endif
  }

  inline SocketHandle(FromHandle from) : socket_(from.handle) {
    if (socket_ < 0) {
      CURRENT_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    BRICKS_NET_LOG("S%05d == initialized externally.\n", socket_);
  }

  inline ~SocketHandle() {
    if (socket_ != static_cast<SOCKET>(-1)) {
      BRICKS_NET_LOG("S%05d close() ...\n", socket_);
#ifndef CURRENT_WINDOWS
      ::shutdown(socket, SHUT_RDWR);
      ::close(socket_);
#else
      ::shutdown(socket, SD_BOTH);
      ::closesocket(socket_);
#endif
      BRICKS_NET_LOG("S%05d close() : OK\n", socket_);
    }
  }

  inline explicit SocketHandle(SocketHandle&& rhs) : socket_(static_cast<SOCKET>(-1)) {
    std::swap(socket_, rhs.socket_);
  }

  // SocketHandle does not expose copy constructor and assignment operator. It should only be moved.

 private:
#ifndef CURRENT_WINDOWS
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
        CURRENT_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
      }
      if (ref_ == static_cast<SOCKET>(-1)) {
        CURRENT_THROW(AttemptedToUseMovedAwayConnection());
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

#ifndef CURRENT_WINDOWS
  SocketHandle(const SocketHandle&) = delete;
#else
 public:
  // Visual Studio seem to require this constructor, since std::move()-ing away `SocketHandle`-s
  // as part of `Connection` objects doesn't seem to work. TODO(dkorolev): Investigate this.
  SocketHandle(const SocketHandle& rhs) : socket_(static_cast<SOCKET>(-1)) { std::swap(socket_, rhs.socket_); }

 private:
#endif
};

struct IPAndPort {
  std::string ip;
  int port;
  IPAndPort(const std::string& ip = "", int port = 0) : ip(ip), port(port) {}
  IPAndPort(const IPAndPort&) = default;
  IPAndPort(IPAndPort&&) = default;
  IPAndPort& operator=(const IPAndPort&) = default;
  IPAndPort& operator=(IPAndPort&&) = default;
};

inline std::string InetAddrToString(const struct in_addr* in) {
  // 16 bytes buffer for IPv4.
  char buffer[16];
#ifndef CURRENT_WINDOWS
  const char* result = ::inet_ntop(AF_INET, reinterpret_cast<const void*>(in), buffer, sizeof(buffer));
#else
  const char* result =
      ::inet_ntop(AF_INET, const_cast<void*>(reinterpret_cast<const void*>(in)), buffer, sizeof(buffer));
#endif  // !CURRENT_WINDOWS
  if (!result) {
    CURRENT_THROW(InetAddrToStringException());
  } else {
    return std::string(result);
  }
}

class Connection : public SocketHandle {
 public:
  Connection(SocketHandle&& rhs, IPAndPort&& local_ip_and_port, IPAndPort&& remote_ip_and_port)
      : SocketHandle(std::move(rhs)),
        local_ip_and_port_(std::move(local_ip_and_port)),
        remote_ip_and_port_(std::move(remote_ip_and_port)) {}

  Connection(Connection&& rhs) = default;

  const IPAndPort& LocalIPAndPort() const { return local_ip_and_port_; }

  const IPAndPort& RemoteIPAndPort() const { return remote_ip_and_port_; }

  // By default, BlockingRead() will return as soon as some data has been read,
  // with the exception being multibyte records (sizeof(T) > 1), where it will keep reading
  // until the boundary of the records, or max_length of them, has been read.
  // Alternatively, the 3rd parameter can be explicitly set to BlockingReadPolicy::FillFullBuffer,
  // which will cause BlockingRead() to keep reading more data until all `max_elements` are read.
  enum BlockingReadPolicy { ReturnASAP = false, FillFullBuffer = true };
  template <typename T>
  inline ENABLE_IF<sizeof(T) == 1, size_t> BlockingRead(
      T* output_buffer, size_t max_length, BlockingReadPolicy policy = BlockingReadPolicy::ReturnASAP) {
    if (max_length == 0) {
      return 0;  // LCOV_EXCL_LINE
    } else {
      uint8_t* buffer = reinterpret_cast<uint8_t*>(output_buffer);
      uint8_t* ptr = buffer;
      const uint8_t* end = (buffer + max_length);
      const int flags = ((policy == BlockingReadPolicy::ReturnASAP) ? 0 : MSG_WAITALL);

#ifdef CURRENT_WINDOWS
      int wsa_last_error = 0;
#endif

      BRICKS_NET_LOG("S%05d BlockingRead() ...\n", static_cast<SOCKET>(socket));
      do {
        const size_t remaining_bytes_to_read = (max_length - (ptr - buffer));
        BRICKS_NET_LOG("S%05d BlockingRead() ... attempting to read %d bytes.\n",
                       static_cast<SOCKET>(socket),
                       static_cast<int>(remaining_bytes_to_read));
#ifndef CURRENT_WINDOWS
        const ssize_t retval = ::recv(socket, ptr, remaining_bytes_to_read, flags);
#else
        const int retval =
            ::recv(socket, reinterpret_cast<char*>(ptr), static_cast<int>(remaining_bytes_to_read), flags);
#endif
        BRICKS_NET_LOG("S%05d BlockingRead() ... retval = %d, errno = %d.\n",
                       static_cast<SOCKET>(socket),
                       static_cast<int>(retval),
                       errno);
        if (retval > 0) {
          ptr += retval;
          if ((policy == BlockingReadPolicy::ReturnASAP) || (ptr == end)) {
            return (ptr - buffer);
          } else {
            continue;  // LCOV_EXCL_LINE
          }
        }

#ifdef CURRENT_WINDOWS
        wsa_last_error = ::WSAGetLastError();
#endif

#ifndef CURRENT_WINDOWS
        if (errno == EAGAIN) {  // LCOV_EXCL_LINE
          continue;             // LCOV_EXCL_LINE
        }
#else
        if (wsa_last_error == WSAEWOULDBLOCK || wsa_last_error == WSAEINPROGRESS ||
            wsa_last_error == WSAENETDOWN) {
          // Effectively, `errno == EAGAIN`.
          continue;
        }
#endif
        // Only keep looping via `continue`.
        // There are two ways:
        // 1) `EAGAIN` or a Windows equivalent has been returned.
        // 2) `retval > 0`, not all the data has been read, and mode is `BlockingReadPolicy::FillFullBuffer`.
        //    (Really, just a safety check).
      } while (false);

// LCOV_EXCL_START
#ifndef CURRENT_WINDOWS
      if (errno == ECONNRESET) {
        if (ptr != buffer) {
          BRICKS_NET_LOG("S%05d BlockingRead() : Connection reset by peer after reading %d bytes.\n",
                         static_cast<SOCKET>(socket),
                         static_cast<int>(ptr - buffer));
          CURRENT_THROW(ConnectionResetByPeer());
        } else {
          CURRENT_THROW(EmptyConnectionResetByPeer());
        }
      } else {
        if (ptr != buffer) {
          BRICKS_NET_LOG("S%05d BlockingRead() : Error after reading %d bytes, errno %d.\n",
                         static_cast<SOCKET>(socket),
                         static_cast<int>(ptr - buffer),
                         errno);
          CURRENT_THROW(SocketReadException());
        } else {
          CURRENT_THROW(EmptySocketReadException());
        }
      }
#else
      if (wsa_last_error == WSAECONNRESET) {
        // Effectively, `errno == ECONNRESET`.
        if (ptr != buffer) {
          BRICKS_NET_LOG("S%05d BlockingRead() : Connection reset by peer after reading %d bytes.\n",
                         static_cast<SOCKET>(socket),
                         static_cast<int>(ptr - buffer));
          CURRENT_THROW(ConnectionResetByPeer());
        } else {
          CURRENT_THROW(EmptyConnectionResetByPeer());
        }
      } else {
        if (ptr != buffer) {
          BRICKS_NET_LOG("S%05d BlockingRead() : Error after reading %d bytes, errno %d.\n",
                         static_cast<SOCKET>(socket),
                         static_cast<int>(ptr - buffer),
                         errno);
          CURRENT_THROW(SocketReadException());
        } else {
          CURRENT_THROW(EmptySocketReadException());
        }
      }
#endif
      // LCOV_EXCL_STOP
    }
  }

  inline Connection& BlockingWrite(const void* buffer, size_t write_length, bool more) {
#if defined(CURRENT_APPLE) || defined(CURRENT_WINDOWS)
    static_cast<void>(more);  // Supress the 'unused parameter' warning.
#endif
    assert(buffer);
    BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) ...\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE)
    const int result =
        static_cast<int>(::send(socket, buffer, write_length, MSG_NOSIGNAL | (more ? MSG_MORE : 0)));
#else
    // No `MSG_NOSIGNAL` and extra cast for Visual Studio.
    // (As I understand, Windows sockets would not result in pipe-related issues. -- D.K.)
    const int result =
        static_cast<int>(::send(socket, static_cast<const char*>(buffer), static_cast<int>(write_length), 0));
#endif
    if (result < 0) {
      CURRENT_THROW(SocketWriteException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    } else if (static_cast<size_t>(result) != write_length) {
      CURRENT_THROW(SocketCouldNotWriteEverythingException());  // This one is tested though.
    }
    BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) : OK\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
    return *this;
  }

  inline Connection& BlockingWrite(const char* s, bool more) {
    assert(s);
    return BlockingWrite(s, strlen(s), more);
  }

  template <typename T>
  inline Connection& BlockingWrite(const T begin, const T end, bool more) {
    if (begin != end) {
      return BlockingWrite(&(*begin), (end - begin) * sizeof(typename T::value_type), more);
    } else {
      return *this;
    }
  }

  // Specialization for STL containers to allow calling BlockingWrite() on std::string, std::vector, etc.
  // The `std::enable_if<>` clause is required, otherwise `BlockingWrite(char[N])` becomes ambiguous.
  template <typename T>
  inline ENABLE_IF<sizeof(typename T::value_type) != 0> BlockingWrite(const T& container, bool more) {
    BlockingWrite(container.begin(), container.end(), more);
  }

 private:
  const IPAndPort local_ip_and_port_;
  const IPAndPort remote_ip_and_port_;

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
      : SocketHandle(SocketHandle::NewHandle(), disable_nagle_algorithm) {
    sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    // Catch a level 4 warning of MSVS.
    addr_server.sin_port = htons(static_cast<decltype(std::declval<sockaddr_in>().sin_port)>(port));

    BRICKS_NET_LOG("S%05d bind()+listen() ...\n", static_cast<SOCKET>(socket));

    if (::bind(socket, reinterpret_cast<sockaddr*>(&addr_server), sizeof(addr_server)) ==
        static_cast<SOCKET>(-1)) {
      CURRENT_THROW(SocketBindException());
    }

    BRICKS_NET_LOG("S%05d bind()+listen() : bind() OK\n", static_cast<SOCKET>(socket));

    if (::listen(socket, max_connections)) {
      CURRENT_THROW(SocketListenException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

    BRICKS_NET_LOG("S%05d bind() and listen() : listen() OK\n", static_cast<SOCKET>(socket));
  }

  Socket(Socket&&) = default;

  inline Connection Accept() {
    BRICKS_NET_LOG("S%05d accept() ...\n", static_cast<SOCKET>(socket));
    sockaddr_in addr_client;
    memset(&addr_client, 0, sizeof(addr_client));

#ifndef CURRENT_WINDOWS
    socklen_t addr_client_length = sizeof(sockaddr_in);
    const auto invalid_socket = static_cast<SOCKET>(-1);
#else
    int addr_client_length = sizeof(sockaddr_in);
    const auto invalid_socket = INVALID_SOCKET;
#endif
    const SOCKET handle =
        ::accept(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length);
    if (handle == invalid_socket) {
      BRICKS_NET_LOG("S%05d accept() : Failed.\n", static_cast<SOCKET>(socket));
      CURRENT_THROW(SocketAcceptException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
    BRICKS_NET_LOG("S%05d accept() : OK, FD = %d.\n", static_cast<SOCKET>(socket), handle);

    sockaddr_in addr_serv;
#ifndef CURRENT_WINDOWS
    socklen_t addr_serv_length = sizeof(sockaddr_in);
#else
    int addr_serv_length = sizeof(sockaddr_in);
#endif
    if (::getsockname(handle, reinterpret_cast<struct sockaddr*>(&addr_serv), &addr_serv_length) != 0) {
      CURRENT_THROW(SocketGetSockNameException());
    }
    IPAndPort local(InetAddrToString(&addr_serv.sin_addr), ntohs(addr_serv.sin_port));
    IPAndPort remote(InetAddrToString(&addr_client.sin_addr), ntohs(addr_client.sin_port));
    return Connection(SocketHandle::FromHandle(handle), std::move(local), std::move(remote));
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

// Smart wrapper for system-allocated `addrinfo` pointers.
namespace addrinfo_t_impl {
struct Deleter {
  void operator()(struct addrinfo* ptr) { ::freeaddrinfo(ptr); }
};
}
using addrinfo_t = std::unique_ptr<struct addrinfo, addrinfo_t_impl::Deleter>;

inline addrinfo_t GetAddrInfo(const std::string& host, const std::string& serv = "") {
  struct addrinfo* result = nullptr;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  const int retval = ::getaddrinfo(host.c_str(), serv.c_str(), &hints, &result);
  if (!result) {
    CURRENT_THROW(SocketResolveAddressException());
  } else if (retval) {
    freeaddrinfo(result);
    CURRENT_THROW(SocketResolveAddressException(gai_strerror(retval)));
  }
  return addrinfo_t(result);
}

inline std::string ResolveIPFromHostname(const std::string& hostname) {
  auto addr_info = GetAddrInfo(hostname);
  // NOTE: Using the first known IP.
  struct sockaddr_in* p_addr_in = reinterpret_cast<struct sockaddr_in*>(addr_info->ai_addr);
  return InetAddrToString(&(p_addr_in->sin_addr));
}

// POSIX allows numeric ports, as well as strings like "http".
template <typename T>
inline Connection ClientSocket(const std::string& host, T port_or_serv) {
  class ClientSocket final : public SocketHandle {
   public:
    inline explicit ClientSocket(const std::string& host, const std::string& serv)
        : SocketHandle(SocketHandle::NewHandle()) {
      BRICKS_NET_LOG("S%05d ", static_cast<SOCKET>(socket));
      // Deliberately left non-const because of possible Windows issues. -- M.Z.
      auto addr_info = GetAddrInfo(host, serv);
      struct sockaddr* p_addr = addr_info->ai_addr;
      // TODO(dkorolev): Use a random address, not the first one. Ref. iteration:
      // for (struct addrinfo* p = servinfo; p != NULL; p = p->ai_next) {
      //   p->ai_addr;
      // }
      struct sockaddr_in* p_addr_in = reinterpret_cast<struct sockaddr_in*>(p_addr);
      remote_ip_and_port.ip = InetAddrToString(&(p_addr_in->sin_addr));
      remote_ip_and_port.port = htons(p_addr_in->sin_port);

      BRICKS_NET_LOG("S%05d connect() ...\n", static_cast<SOCKET>(socket));
      const int retval = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval) {
        CURRENT_THROW(SocketConnectException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
      }

#ifndef CURRENT_WINDOWS
      socklen_t addr_client_length = sizeof(sockaddr_in);
#else
      int addr_client_length = sizeof(sockaddr_in);
#endif
      sockaddr_in addr_client;
      if (::getsockname(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length) != 0) {
        CURRENT_THROW(SocketGetSockNameException());
      }
      local_ip_and_port.ip = InetAddrToString(&addr_client.sin_addr);
      local_ip_and_port.port = htons(addr_client.sin_port);

      BRICKS_NET_LOG("S%05d connect() OK\n", static_cast<SOCKET>(socket));
    }
    IPAndPort local_ip_and_port;
    IPAndPort remote_ip_and_port;
  };
  auto client_socket = ClientSocket(host, std::to_string(port_or_serv));
  IPAndPort local_ip_and_port(std::move(client_socket.local_ip_and_port));
  IPAndPort remote_ip_and_port(std::move(client_socket.remote_ip_and_port));
  return Connection(std::move(client_socket), std::move(local_ip_and_port), std::move(remote_ip_and_port));
}

}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_TCP_IMPL_POSIX_H
