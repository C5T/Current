// Bricks HTTP API provides a generic interface for basic HTTP operations,
// along with their cross-platform implementations.
//
// HTTP server has a TODO(dkorolev) implementation.
// HTTP client has POSIX, MacOS/iOS and Android/Java implementations.
//
// The user does not have to select the implementation, a suitable one will be selected at compile time.
//
// TODO(dkorolev): Either port the Java test here from AlohaStatsClient and make it pass, or get rid of it.
//
// Synopsis:
//
// # CLIENT: Returns template-specialized type depending on its parameters. Best to save into `const auto`.
//
// ## const std::string& s = HTTP(GET(url)).body;
// ## const auto r = HTTP(GET(url)); DoWork(r.code, r.body);
// ## const auto r = HTTP(GET(url), SaveResponseToFile(file_name)); DoWork(r.code, r.body_file_name);
// ## const auto r = HTTP(POST(url, "data", "text/plain")); DoWork(r.code);
// ## const auto r = HTTP(POSTFromFile(url, file_name, "text/plain")); DoWork(r.code);
//                   TODO(dkorolev): Hey Alex, do we support returned body from POST requests? :-)
//
// # SERVER: TODO(dkorolev).

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
