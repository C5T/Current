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

#include "../exception.h"

namespace bricks {
namespace net {

// All exceptions are derived from NetworkException.
struct NetworkException : Exception {};

// TCP-level exceptions are derived from SocketException.
struct SocketException : NetworkException {};

struct InvalidSocketException : SocketException {};
struct SocketCreateException : SocketException {};

struct ServerSocketException : SocketException {};
struct SocketBindException : ServerSocketException {};
struct SocketListenException : ServerSocketException {};
struct SocketAcceptException : ServerSocketException {};

struct ClientSocketException : SocketException {};
struct SocketConnectException : ClientSocketException {};
struct SocketResolveAddressException : ClientSocketException {};

struct SocketFcntlException : SocketException {};
struct SocketReadException : SocketException {};
struct SocketReadMultibyteRecordEndedPrematurelyException : SocketReadException {};
struct SocketWriteException : SocketException {};
struct SocketCouldNotWriteEverythingException : SocketWriteException {};

// HTTP-level exceptions are derived from HTTPException.
struct HTTPException : NetworkException {};

struct HTTPConnectionClosedByPeerException : HTTPException {};
struct HTTPNoBodyProvidedException : HTTPException {};
struct HTTPRedirectNotAllowedException : HTTPException {};
struct HTTPRedirectLoopException : HTTPException {};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_EXCEPTIONS_H
