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
struct HTTPRedirectLoopException : HTTPException {};

}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_EXCEPTIONS_H
