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

namespace bricks {
namespace net {

// All exceptions are derived from NetworkException.
struct NetworkException : Exception {
  NetworkException() {}
  NetworkException(const std::string& what) : Exception(what) {}
};

// TCP-level exceptions are derived from SocketException.
struct SocketException : NetworkException {};

#ifdef BRICKS_WINDOWS
struct SocketWSAStartupException : SocketException {};
#endif

struct InvalidSocketException : SocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct AttemptedToUseMovedAwayConnection : SocketException {};
struct SocketCreateException : SocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.

struct ServerSocketException : SocketException {};
struct SocketBindException : ServerSocketException {};
struct SocketListenException : ServerSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct SocketAcceptException : ServerSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.

struct ConnectionResetByPeer : SocketException {};  // LCOV_EXCL_LINE

struct ClientSocketException : SocketException {};
struct SocketConnectException : ClientSocketException {};  // LCOV_EXCL_LINE -- not covered by unit tests.
struct SocketResolveAddressException : ClientSocketException {};

struct SocketFcntlException : SocketException {};
struct SocketReadException : SocketException {};  // LCOV_EXCL_LINE -- TODO(dkorolev): We might want to test it.
struct SocketWriteException : SocketException {};
struct SocketCouldNotWriteEverythingException : SocketWriteException {};

// HTTP-level exceptions are derived from HTTPException.
struct HTTPException : NetworkException {
  HTTPException() {}
  HTTPException(const std::string& what) : NetworkException(what) {}
};

struct HTTPNoBodyProvidedException : HTTPException {};
struct HTTPRedirectNotAllowedException : HTTPException {};
struct HTTPRedirectLoopException : HTTPException {};
struct CannotServeStaticFilesOfUnknownMIMEType : HTTPException {
  CannotServeStaticFilesOfUnknownMIMEType(const std::string& what) : HTTPException(what) {}
};

// AttemptedToSendHTTPResponseMoreThanOnce is a user code exception; not really an HTTP one.
struct AttemptedToSendHTTPResponseMoreThanOnce : Exception {};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_EXCEPTIONS_H
