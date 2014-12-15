// Bricks HTTP API provides a generic interface for basic HTTP operations,
// along with their cross-platform implementations.
//
// See the header comment in types.h for synopsis.

#ifndef BRICKS_NET_API_API_H
#define BRICKS_NET_API_API_H

#include "../../port.h"

#include "types.h"

#if defined(BRICKS_POSIX)
#include "impl/posix.h"
typedef bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientPOSIX> HTTP_TYPE;
#elif defined(BRICKS_APPLE)
#include "impl/apple.h"
typedef bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientApple> HTTP_TYPE;
#elif defined(BRICKS_JAVA) || defined(BRICKS_ANDROID)
#include "impl/java.h"
typedef bricks::net::api::HTTPClientImpl<aloha::HTTPClientPlatformWrapper> HTTP_TYPE;
#else
#error "No implementation for `net/api/api.h` is available for your system."
#endif

// TODO(dkorolev): This allows using HTTP(GET(url)) without violating ODR. Think of a better way.
struct HTTPSingleton {
  static HTTP_TYPE& Get() {
    static HTTP_TYPE instance;
    return instance;
  }
};

#define HTTP (HTTPSingleton::Get())

#endif  // BRICKS_NET_API_API_H
