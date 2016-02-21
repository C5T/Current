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

// Current HTTP API provides a generic interface for basic HTTP operations,
// along with their cross-platform implementations.
//
// HTTP server has a POSIX implementation.
// HTTP client has POSIX, MacOS/iOS and Android/Java implementations (the Java one is untested for a while).
//
// The user does not have to select the implementation, a suitable one will be selected at compile time.

#ifndef BLOCKS_HTTP_API_H
#define BLOCKS_HTTP_API_H

#include "../../port.h"

#include "request.h"
#include "response.h"
#include "types.h"

#include "../../Bricks/util/singleton.h"
#include "../../Bricks/template/weed.h"

#if defined(CURRENT_POSIX) || defined(CURRENT_WINDOWS)
#include "impl/posix_client.h"
#include "impl/posix_server.h"
using HTTP_CLIENT = current::http::HTTPClientPOSIX;
#elif defined(CURRENT_APPLE)
#include "impl/apple_client.h"
#include "impl/posix_server.h"
using HTTP_CLIENT = current::http::HTTPClientApple;
#elif defined(CURRENT_JAVA) || defined(CURRENT_ANDROID)
#include "impl/java_client.h"
#include "impl/posix_server.h"
using HTTP_CLIENT = java_wrapper::HTTPClientPlatformWrapper;
#else
#error "No implementation for `net/api/api.h` is available for your system."
#endif

using HTTP_IMPL = current::http::HTTPImpl<HTTP_CLIENT, current::http::HTTPServerPOSIX>;

namespace current {
namespace http {

template <typename... TS>
inline typename current::weed::call_with_type<HTTP_IMPL, TS...> HTTP(TS&&... params) {
  return current::Singleton<HTTP_IMPL>()(std::forward<TS>(params)...);
}

}  // namespace http
}  // namespace current

using current::http::HTTP;
using current::http::Request;
using current::http::Response;
using current::http::ReRegisterRoute;
using HTTPRoutesScope = typename HTTP_IMPL::T_SERVER_IMPL::HTTPRoutesScope;
using HTTPRoutesScopeEntry = current::http::HTTPServerPOSIX::HTTPRoutesScopeEntry;

#endif  // BLOCKS_HTTP_API_H
