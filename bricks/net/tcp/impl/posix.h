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

#if 0

#NOTE(dkorolev) : I have tested the `ReserveLocalPort` logic in the following way.
#Step one : build.
g++ -g -std=c++17 -W -Wall -Wno-strict-aliasing   -o ".current/test" "test.cc" -pthread -ldl
ulimit -c unlimited
#Step two : run in two terminals in parallel.
rm -f core ; while true ; do ./.current/test --gtest_throw_on_failure --gtest_catch_exceptions=0 || break ; done

#endif

// TODO(dkorolev): Add Mac support and find out the right name for this header file.

#ifndef BRICKS_NET_TCP_IMPL_POSIX_H
#define BRICKS_NET_TCP_IMPL_POSIX_H

#include <type_traits>

#ifndef CURRENT_WINDOWS

#include <errno.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

// Bricks uses `SOCKET` for socket handles in *nix.
// Makes it easier to have the code run on both Windows and *nix.
// NOTE(dkorolev): Some irresponsible elements `#define SOCKET int` in their code,
//                 and we must protect the life forms to which we are superior.
#ifndef SOCKET
typedef int SOCKET;
#endif

#else

#include <WS2tcpip.h>

#include <corecrt_io.h>
#pragma comment(lib, "Ws2_32.lib")

#endif  // CURRENT_WINDOWS

#include <iostream>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "../../exceptions.h"
#include "../../debug_log.h"

#include "../../../util/random.h"
#include "../../../util/singleton.h"

namespace current {
namespace net {

enum class NagleAlgorithm : bool { Disable, Keep };
const NagleAlgorithm kDefaultNagleAlgorithmPolicy = NagleAlgorithm::Keep;

enum class MaxServerQueuedConnectionsValue : int {};
const MaxServerQueuedConnectionsValue kMaxServerQueuedConnections = MaxServerQueuedConnectionsValue(1024);

struct SocketSystemInitializer {
#ifdef CURRENT_WINDOWS
  struct OneTimeInitializer {
    OneTimeInitializer() {
      CURRENT_BRICKS_NET_LOG("WSAStartup() ...\n");
      WSADATA wsaData;
      if (WSAStartup(MAKEWORD(2, 2), &wsaData) != NO_ERROR) {
        CURRENT_THROW(SocketWSAStartupException());
      }
      CURRENT_BRICKS_NET_LOG("WSAStartup() : OK\n");
    }
  };
  SocketSystemInitializer() { Singleton<OneTimeInitializer>(); }
#endif  // CURRENT_WINDOWS
};

enum class BarePort : uint16_t {};

class SocketHandle : private SocketSystemInitializer {
 private:
  struct InternalInit final {};
  explicit SocketHandle(InternalInit, NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy)
      : socket_(::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) {
    if (socket_ < 0) {
      CURRENT_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    CURRENT_BRICKS_NET_LOG("S%05d socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);\n", socket_);

#ifndef CURRENT_WINDOWS
    int just_one = 1;
#else
    u_long just_one = 1;
#endif  // CURRENT_WINDOWS

#ifndef CURRENT_WINDOWS
    // NOTE(dkorolev): On Windows, `SO_REUSEADDR` is unnecessary.
    // First, local ports can be reused right away, so it doesn't win anything.
    // Second, on Windows this setting allows binding the second socket to the same port, which is just bad.
    if (::setsockopt(socket_, SOL_SOCKET, SO_REUSEADDR, &just_one, sizeof(just_one))) {
      CURRENT_THROW(SocketCreateException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
#endif

#ifdef CURRENT_APPLE
    // Emulate MSG_NOSIGNAL behavior.
    if (::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, static_cast<void*>(&just_one), sizeof(just_one))) {
      CURRENT_THROW(SocketCreateException());
    }
#endif

    // LCOV_EXCL_START
    if (nagle_algorithm_policy == NagleAlgorithm::Disable) {
#ifndef CURRENT_WINDOWS
      if (::setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, &just_one, sizeof(just_one)))
#else
      if (::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&just_one), sizeof(just_one)))
#endif  // CURRENT_WINDOWS
      {
        CURRENT_THROW(SocketCreateException());
      }
    }
    // LCOV_EXCL_STOP
  }

 public:
  struct BindAndListen final {};
  explicit SocketHandle(BindAndListen,
                        BarePort bare_port,
                        NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
                        MaxServerQueuedConnectionsValue max_connections = kMaxServerQueuedConnections)
      : SocketHandle(InternalInit(), nagle_algorithm_policy) {
    sockaddr_in addr_server;
    memset(&addr_server, 0, sizeof(addr_server));
    addr_server.sin_family = AF_INET;
    addr_server.sin_addr.s_addr = INADDR_ANY;
    // Catch a level 4 warning of MSVS.
    addr_server.sin_port = htons(static_cast<decltype(std::declval<sockaddr_in>().sin_port)>(bare_port));

    CURRENT_BRICKS_NET_LOG("S%05d bind()+listen() ...\n", static_cast<SOCKET>(socket));

    if (::bind(socket, reinterpret_cast<sockaddr*>(&addr_server), sizeof(addr_server)) == static_cast<SOCKET>(-1)) {
      CURRENT_THROW(SocketBindException(static_cast<uint16_t>(bare_port)));
    }

    CURRENT_BRICKS_NET_LOG("S%05d bind()+listen() : bind() OK\n", static_cast<SOCKET>(socket));

    if (::listen(socket, static_cast<int>(max_connections))) {
      CURRENT_THROW(SocketListenException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }

    CURRENT_BRICKS_NET_LOG("S%05d bind() and listen() : listen() OK\n", static_cast<SOCKET>(socket));
  }

  struct DoNotBind final {};
  explicit SocketHandle(DoNotBind, NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy)
      : SocketHandle(InternalInit(), nagle_algorithm_policy) {}

  struct FromAcceptedHandle final {
    SOCKET handle;
    FromAcceptedHandle(SOCKET handle) : handle(handle) {}
  };
  SocketHandle(FromAcceptedHandle from) : socket_(from.handle) {
    if (socket_ < 0) {
      CURRENT_THROW(InvalidSocketException());  // LCOV_EXCL_LINE -- Not covered by unit tests.
    }
    CURRENT_BRICKS_NET_LOG("S%05d == initialized externally.\n", socket_);
  }

  ~SocketHandle() {
    if (socket_ != static_cast<SOCKET>(-1)) {
      CURRENT_BRICKS_NET_LOG("S%05d close() ...\n", socket_);
#ifndef CURRENT_WINDOWS
      ::shutdown(socket, SHUT_RDWR);
      ::close(socket_);
#else
      ::shutdown(socket, SD_BOTH);
      ::closesocket(socket_);
#endif  // CURRENT_WINDOWS
      CURRENT_BRICKS_NET_LOG("S%05d close() : OK\n", socket_);
    }
  }

  explicit SocketHandle(SocketHandle&& rhs) : socket_(static_cast<SOCKET>(-1)) { std::swap(socket_, rhs.socket_); }

 private:
  friend class Socket;
  SOCKET socket_;

 public:
  // The `ReadOnlyValidSocketAccessor socket` members provide simple read-only access to `socket_` via `socket`.
  // It is kept here to aggressively restrict access to `socket_` from the outside,
  // since debugging move-constructed socket handles has proven to be nontrivial -- D.K.
  class ReadOnlyValidSocketAccessor final {
   public:
    explicit ReadOnlyValidSocketAccessor(const SOCKET& ref) : ref_(ref) {}
    operator SOCKET() {
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
  SocketHandle(const SocketHandle&) = delete;
  void operator=(const SocketHandle&) = delete;
  void operator=(SocketHandle&& rhs) {
    socket_ = rhs.socket_;
    rhs.socket_ = static_cast<SOCKET>(-1);
  }
};

struct IPAndPort {
  std::string ip;
  uint16_t port;
  IPAndPort(const std::string& ip = "", uint16_t port = 0) : ip(ip), port(port) {}
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

// NOTE(dkorolev): These are magic numbers, might use some SFINAE and DECLARE_* to make them flags-configurable later.
constexpr static uint16_t kPickFreePortMin = 25000;
constexpr static uint16_t kPickFreePortMax = 29000;

class ReservedLocalPort final : public current::net::SocketHandle {
 private:
  using super_t = current::net::SocketHandle;
  friend class Socket;
  uint16_t port_;

 public:
  struct Construct final {};
  ReservedLocalPort() = delete;
  ReservedLocalPort(Construct, uint16_t port, current::net::SocketHandle&& socket)
      : super_t(std::move(socket)), port_(port) {}
  ReservedLocalPort(const ReservedLocalPort&) = delete;
  ReservedLocalPort(ReservedLocalPort&& rhs) : super_t(std::move(rhs)), port_(rhs.port_) { rhs.port_ = 0; }
  ReservedLocalPort& operator=(const ReservedLocalPort&) = delete;
  ReservedLocalPort& operator=(ReservedLocalPort&&) = delete;
  operator uint16_t() const { return port_; }

  // This one is for the tests, for `%d`-s to work in test "URL"-s construction without any `static_cast<>`-s.
  operator int() const { return static_cast<int>(port_); }
};

namespace impl {

class ReserveLocalPortImpl final {
 private:
  std::vector<uint16_t> order_;
  size_t index_;

 public:
  ReserveLocalPortImpl() {
    for (uint16_t port = kPickFreePortMin; port <= kPickFreePortMax; ++port) {
      order_.push_back(port);
    }
    index_ = 0u;
  }

  current::net::ReservedLocalPort DoIt(uint16_t hint,
                                       NagleAlgorithm nagle_algorithm_policy,
                                       MaxServerQueuedConnectionsValue max_connections) {
    size_t save_index = index_;
    bool keep_searching = true;
    while (keep_searching) {
      if (!index_) {
        std::shuffle(std::begin(order_), std::end(order_), current::random::mt19937_64_tls());
      }
      const uint16_t candidate_port = [&]() {
        uint16_t retval;
        if (hint) {
          retval = hint;
          hint = 0u;
        } else {
          retval = order_[index_++];
          if (index_ == order_.size()) {
            index_ = 0u;
          }
          if (index_ == save_index) {
            keep_searching = false;
          }
        }
        return retval;
      }();
      try {
        current::net::SocketHandle try_to_hold_port(current::net::SocketHandle::BindAndListen(),
                                                    BarePort(candidate_port),
                                                    nagle_algorithm_policy,
                                                    max_connections);
        return current::net::ReservedLocalPort(
            current::net::ReservedLocalPort::Construct(), candidate_port, std::move(try_to_hold_port));
      } catch (const current::net::SocketConnectException&) {
        // Keep trying.
        // std::cerr << "Failed in `connect`." << candidate_port << '\n';
      } catch (const current::net::SocketBindException&) {
        // Keep trying.
        // std::cerr << "Failed in `bind`." << candidate_port << '\n';
      } catch (const current::net::SocketListenException&) {
        // Keep trying.
        // std::cerr << "Failed in `listen`." << candidate_port << '\n';
      }
      // Consciously fail on other exception types here.
    }
    std::cerr << "FATAL ERROR: Failed to pick an available local port." << std::endl;
    std::exit(-1);
  }
};

}  // namespace impl

// Pick an available local port.
[[nodiscard]] inline ReservedLocalPort ReserveLocalPort(
    NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
    MaxServerQueuedConnectionsValue max_connections = kMaxServerQueuedConnections) {
  return current::ThreadLocalSingleton<impl::ReserveLocalPortImpl>().DoIt(0, nagle_algorithm_policy, max_connections);
}

[[nodiscard]] inline ReservedLocalPort ReserveLocalPort(
    uint16_t hint,
    NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
    MaxServerQueuedConnectionsValue max_connections = kMaxServerQueuedConnections) {
  return current::ThreadLocalSingleton<impl::ReserveLocalPortImpl>().DoIt(
      hint, nagle_algorithm_policy, max_connections);
}

// Acquires the local port with the explicltly provided number, or throws the exception.
// Used to cleanly construct the HTTP server on the specific port number,
// or fail early, from the thread that is attempting to construct this HTTP server.
// The alternative way used before has the issue that the port is acquired in the thread, not outside it,
// and catching the exception that might occur during its construction might not do what the user expected it to do.
// TODO(dkorolev): Maybe rewrite the default logic as well and retire the `safe_http_construction` example?
[[nodiscard]] inline ReservedLocalPort AcquireLocalPort(
    uint16_t port,
    NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
    MaxServerQueuedConnectionsValue max_connections = kMaxServerQueuedConnections) {
  auto hold_port_or_throw = current::net::SocketHandle(current::net::SocketHandle::BindAndListen(),
                                                current::net::BarePort(port),
                                                nagle_algorithm_policy,
                                                max_connections);
  return current::net::ReservedLocalPort(current::net::ReservedLocalPort::Construct(),
                                         port,
                                         std::move(hold_port_or_throw));
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
  inline std::enable_if_t<sizeof(T) == 1, size_t> BlockingRead(
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

      CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() ...\n", static_cast<SOCKET>(socket));
      do {
        const size_t remaining_bytes_to_read = (max_length - (ptr - buffer));
        CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() ... attempting to read %d bytes.\n",
                               static_cast<SOCKET>(socket),
                               static_cast<int>(remaining_bytes_to_read));
#ifndef CURRENT_WINDOWS
        const ssize_t retval = ::recv(socket, ptr, remaining_bytes_to_read, flags);
#else
        const int retval =
            ::recv(socket, reinterpret_cast<char*>(ptr), static_cast<int>(remaining_bytes_to_read), flags);
#endif  // CURRENT_WINDOWS
        CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() ... retval = %d, errno = %d.\n",
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

#ifndef CURRENT_WINDOWS
        if (errno == EAGAIN) {  // LCOV_EXCL_LINE
          continue;             // LCOV_EXCL_LINE
        }
#else
        wsa_last_error = ::WSAGetLastError();
        if (wsa_last_error == WSAEWOULDBLOCK || wsa_last_error == WSAEINPROGRESS || wsa_last_error == WSAENETDOWN) {
          // Effectively, `errno == EAGAIN`.
          continue;
        }
#endif  // CURRENT_WINDOWS

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
          CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() : Connection reset by peer after reading %d bytes.\n",
                                 static_cast<SOCKET>(socket),
                                 static_cast<int>(ptr - buffer));
          CURRENT_THROW(ConnectionResetByPeer());
        } else {
          CURRENT_THROW(EmptyConnectionResetByPeer());
        }
      } else {
        if (ptr != buffer) {
          CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() : Error after reading %d bytes, errno %d.\n",
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
          CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() : Connection reset by peer after reading %d bytes.\n",
                                 static_cast<SOCKET>(socket),
                                 static_cast<int>(ptr - buffer));
          CURRENT_THROW(ConnectionResetByPeer());
        } else {
          CURRENT_THROW(EmptyConnectionResetByPeer());
        }
      } else {
        if (ptr != buffer) {
          CURRENT_BRICKS_NET_LOG("S%05d BlockingRead() : Error after reading %d bytes, errno %d.\n",
                                 static_cast<SOCKET>(socket),
                                 static_cast<int>(ptr - buffer),
                                 errno);
          CURRENT_THROW(SocketReadException());
        } else {
          CURRENT_THROW(EmptySocketReadException());
        }
      }
#endif  // CURRENT_WINDOWS
      // LCOV_EXCL_STOP
    }
  }

  Connection& BlockingWrite(const void* buffer, size_t write_length, bool more) {
#if defined(CURRENT_APPLE) || defined(CURRENT_WINDOWS)
    static_cast<void>(more);  // Supress the 'unused parameter' warning.
#endif
    CURRENT_ASSERT(buffer);
    CURRENT_BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) ...\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE)
    const int result = static_cast<int>(::send(socket, buffer, write_length, MSG_NOSIGNAL | (more ? MSG_MORE : 0)));
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
    CURRENT_BRICKS_NET_LOG(
        "S%05d BlockingWrite(%d bytes) : OK\n", static_cast<SOCKET>(socket), static_cast<int>(write_length));
    return *this;
  }

  Connection& BlockingWrite(const char* s, bool more) {
    CURRENT_ASSERT(s);
    return BlockingWrite(s, strlen(s), more);
  }

  template <typename T>
  Connection& BlockingWrite(const T begin, const T end, bool more) {
    if (begin != end) {
      return BlockingWrite(&(*begin), (end - begin) * sizeof(typename T::value_type), more);
    } else {
      return *this;
    }
  }

  // Specialization for STL containers to allow calling BlockingWrite() on std::string, std::vector, etc.
  // The `std::enable_if_t<>` clause is required, otherwise `BlockingWrite(char[N])` becomes ambiguous.
  template <typename T>
  std::enable_if_t<sizeof(typename T::value_type) != 0> BlockingWrite(const T& container, bool more) {
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
  explicit Socket(BarePort bare_port,
                  NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy,
                  MaxServerQueuedConnectionsValue max_connections = kMaxServerQueuedConnections)
      : SocketHandle(SocketHandle::BindAndListen(), bare_port, nagle_algorithm_policy, max_connections) {}

  explicit Socket(ReservedLocalPort&& reserved_port) : SocketHandle(std::move(reserved_port)) {}

  Socket(SocketHandle&& rhs) : SocketHandle(std::move(rhs)) {}
  Socket(Socket&& rhs) : SocketHandle(std::move(rhs)) {}

  Socket& operator=(Socket&& rhs) {
    *static_cast<SocketHandle*>(this) = std::move(rhs);
    rhs.socket_ = static_cast<SOCKET>(-1);
    return *this;
  }

  Socket() = delete;
  Socket(const Socket&) = delete;
  Socket& operator=(const Socket&) = delete;

  Connection Accept() {
    CURRENT_BRICKS_NET_LOG("S%05d accept() ...\n", static_cast<SOCKET>(socket));
    sockaddr_in addr_client;
    memset(&addr_client, 0, sizeof(addr_client));

#ifndef CURRENT_WINDOWS
    socklen_t addr_client_length = sizeof(sockaddr_in);
    const auto invalid_socket = static_cast<SOCKET>(-1);
#else
    int addr_client_length = sizeof(sockaddr_in);
    const auto invalid_socket = INVALID_SOCKET;
#endif  // CURRENT_WINDOWS
    const SOCKET handle = ::accept(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length);
    if (handle == invalid_socket) {
      CURRENT_BRICKS_NET_LOG("S%05d accept() : Failed.\n", static_cast<SOCKET>(socket));
      CURRENT_THROW(SocketAcceptException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
    }
    CURRENT_BRICKS_NET_LOG("S%05d accept() : OK, FD = %d.\n", static_cast<SOCKET>(socket), handle);

    sockaddr_in addr_serv;
#ifndef CURRENT_WINDOWS
    socklen_t addr_serv_length = sizeof(sockaddr_in);
#else
    int addr_serv_length = sizeof(sockaddr_in);
#endif  // CURRENT_WINDOWS
    if (::getsockname(handle, reinterpret_cast<struct sockaddr*>(&addr_serv), &addr_serv_length) != 0) {
      CURRENT_THROW(SocketGetSockNameException());
    }
    IPAndPort local(InetAddrToString(&addr_serv.sin_addr), ntohs(addr_serv.sin_port));
    IPAndPort remote(InetAddrToString(&addr_client.sin_addr), ntohs(addr_client.sin_port));
    return Connection(SocketHandle::FromAcceptedHandle(handle), std::move(local), std::move(remote));
  }
};

// Smart wrapper for system-allocated `addrinfo` pointers.
namespace addrinfo_t_impl {
struct Deleter {
  void operator()(struct addrinfo* ptr) { ::freeaddrinfo(ptr); }
};
}  // namespace addrinfo_t_impl
using addrinfo_t = std::unique_ptr<struct addrinfo, addrinfo_t_impl::Deleter>;

inline addrinfo_t GetAddrInfo(const std::string& host, const std::string& serv = "") {
  const std::string host_and_service = host + (serv.empty() ? "" : " " + serv);
  struct addrinfo* result = nullptr;
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  const int retval = ::getaddrinfo(host.c_str(), serv.c_str(), &hints, &result);
  if (!result) {
    CURRENT_THROW(SocketResolveAddressException(host_and_service));
  } else if (retval) {
    ::freeaddrinfo(result);
    CURRENT_THROW(SocketResolveAddressException(host_and_service + ' ' + ::gai_strerror(retval)));
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
    explicit ClientSocket(const std::string& host,
                          const std::string& serv,
                          NagleAlgorithm nagle_algorithm_policy = kDefaultNagleAlgorithmPolicy)
        : SocketHandle(SocketHandle::DoNotBind(), nagle_algorithm_policy) {
      CURRENT_BRICKS_NET_LOG("S%05d ", static_cast<SOCKET>(socket));
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

      CURRENT_BRICKS_NET_LOG("S%05d connect() ...\n", static_cast<SOCKET>(socket));
      const int retval = ::connect(socket, p_addr, sizeof(*p_addr));
      if (retval) {
        CURRENT_THROW(SocketConnectException());  // LCOV_EXCL_LINE -- Not covered by the unit tests.
      }

#ifndef CURRENT_WINDOWS
      socklen_t addr_client_length = sizeof(sockaddr_in);
#else
      int addr_client_length = sizeof(sockaddr_in);
#endif  // CURRENT_WINDOWS
      sockaddr_in addr_client;
      if (::getsockname(socket, reinterpret_cast<struct sockaddr*>(&addr_client), &addr_client_length) != 0) {
        CURRENT_THROW(SocketGetSockNameException());
      }
      local_ip_and_port.ip = InetAddrToString(&addr_client.sin_addr);
      local_ip_and_port.port = htons(addr_client.sin_port);

      CURRENT_BRICKS_NET_LOG("S%05d connect() OK\n", static_cast<SOCKET>(socket));
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
