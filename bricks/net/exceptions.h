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

#ifndef BRICKS_NET_EXCEPTIONS_H
#define BRICKS_NET_EXCEPTIONS_H

#include "../port.h"
#include "../exception.h"
#include "../strings/util.h"

namespace current {
namespace net {

// All exceptions are derived from NetworkException.
struct NetworkException : Exception {
  NetworkException() {}
  NetworkException(const std::string& what) : Exception(what) {}
};

// TCP-level exceptions are derived from SocketException.
struct SocketException : NetworkException {
  using NetworkException::NetworkException;  // LCOV_EXCL_LINE
};

#ifdef CURRENT_WINDOWS
struct SocketWSAStartupException : SocketException {};
#endif

struct InetAddrToStringException : SocketException {};   // LCOV_EXCL_LINE -- not covered by unit tests.
struct SocketGetSockNameException : SocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.

struct InvalidSocketException : SocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct AttemptedToUseMovedAwayConnection : SocketException {};
struct SocketCreateException : SocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.

struct ServerSocketException : SocketException {
  using SocketException::SocketException;
};

struct SocketBindException : ServerSocketException {
  explicit SocketBindException(uint16_t port) : ServerSocketException(current::ToString(port)) {}
};

struct SocketListenException : ServerSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct SocketAcceptException : ServerSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.

struct ConnectionResetByPeer : SocketException {};  // LCOV_EXCL_LINE

struct ClientSocketException : SocketException {
  using SocketException::SocketException;  // LCOV_EXCL_LINE
};
struct SocketConnectException : ClientSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct SocketResolveAddressException : ClientSocketException {
  using ClientSocketException::ClientSocketException;  // LCOV_EXCL_LINE
};

struct SocketFcntlException : SocketException {};
struct SocketReadException : SocketException {};  // LCOV_EXCL_LINE -- TODO(dkorolev): We might want to test it.
struct SocketWriteException : SocketException {};
struct SocketCouldNotWriteEverythingException : SocketWriteException {};

// We noticed some browsers, Firefox and Chrome included, may pre-open a TCP connection for performance reasons,
// but never send any data. While it is a legitimate case, it results in an annoying warning dumped by Current.
// We suppress those "HTTP route failed" warnings if no data at all was sent in through the socket.

// LCOV_EXCL_START
struct EmptySocketException : SocketException {};
struct EmptyConnectionResetByPeer : EmptySocketException {};
struct EmptySocketReadException : EmptySocketException {};
// LCOV_EXCL_STOP

// HTTP-level exceptions are derived from HTTPException.
struct HTTPException : NetworkException {
  using NetworkException::NetworkException;
};

struct HTTPRedirectNotAllowedException : HTTPException {};
struct HTTPRedirectLoopException : HTTPException {
  using HTTPException::HTTPException;
};
struct HTTPPayloadTooLarge : HTTPException {};
struct HTTPRequestBodyLengthNotProvided : HTTPException {};
struct ChunkSizeNotAValidHEXValue : HTTPException {};
struct ChunkedOrUpgradedHTTPResponseRequestedMismatch : HTTPException {};

// AttemptedToSendHTTPResponseMoreThanOnce is a user code exception; not really an HTTP one.
struct AttemptedToSendHTTPResponseMoreThanOnce : Exception {};

}  // namespace net
}  // namespace current

#endif  // BRICKS_NET_EXCEPTIONS_H
