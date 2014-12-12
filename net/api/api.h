// Bricks HTTP API provides a generic interface for basic HTTP operations,
// along with their cross-platform implementations.
//
// See the header comment in types.h for synopsis.

#ifndef BRICKS_NET_API_H
#define BRICKS_NET_API_H

#include "types.h"

// TODO(dkorolev): Revisit these guards once we go beyond Ubuntu & Mac.
#if defined(__APPLE__)
#include "impl/apple.h"
bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientApple> HTTP;
#elif defined(__linux)
#include "impl/posix.h"
bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientPOSIX> HTTP;
#else
#error "No implementation for `net/api/api.h` is available for your system."
#endif

#endif  // BRICKS_NET_API_H
