/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

namespace bricks {
namespace net {
namespace api {

namespace instance {

struct HTTPHelper {
  static HTTP_TYPE& Singleton() {
    static HTTP_TYPE instance;
    return instance;
  }
};

template <typename T>
inline T TypeHelper() {
  static T* ptr;
  return *ptr;
}

}  // namespace instance

template <typename T1>
inline decltype(instance::HTTPHelper::Singleton()(instance::TypeHelper<T1>())) HTTP(T1&& p1) {
  return instance::HTTPHelper::Singleton()(std::forward<T1>(p1));
}

template <typename T1, typename T2>
inline decltype(instance::HTTPHelper::Singleton()(instance::TypeHelper<T1>(), instance::TypeHelper<T2>())) HTTP(
    T1&& p1, T2&& p2) {
  return instance::HTTPHelper::Singleton()(std::forward<T1>(p1), std::forward<T2>(p2));
}

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_API_H
