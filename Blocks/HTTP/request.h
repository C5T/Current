/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BLOCKS_HTTP_REQUEST_H
#define BLOCKS_HTTP_REQUEST_H

#include <string>

#include "../../Bricks/net/http/http.h"
#include "../../Bricks/time/chrono.h"
#include "../../Bricks/template/decay.h"

namespace blocks {

template <typename T>
constexpr static bool HasRespondViaHTTP(char) {
  return false;
}

template <typename T>
constexpr static auto HasRespondViaHTTP(int)
    -> decltype(std::declval<T>().RespondViaHTTP(std::declval<struct Request>()), bool()) {
  return true;
}

// The only parameter to be passed to HTTP handlers.
struct Request final {
  std::unique_ptr<current::net::HTTPServerConnection> unique_connection;

  current::net::HTTPServerConnection& connection;
  const current::net::HTTPRequestData&
      http_data;  // Accessor to use `r.http_data` instead of `r.connection->HTTPRequest()`.
  const URL& url;
  const std::string method;
  const current::net::HTTPRequestData::HeadersType& headers;
  const std::string& body;  // TODO(dkorolev): This is inefficient, but will do.
  const current::time::EPOCH_MILLISECONDS timestamp;

  explicit Request(std::unique_ptr<current::net::HTTPServerConnection>&& connection)
      : unique_connection(std::move(connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(http_data.URL()),
        method(http_data.Method()),
        headers(http_data.headers()),
        body(http_data.Body()),
        timestamp(current::time::Now()) {}

  // It is essential to move `unique_connection` so that the socket outlives the destruction of `rhs`.
  Request(Request&& rhs)
      : unique_connection(std::move(rhs.unique_connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(http_data.URL()),
        method(http_data.Method()),
        headers(http_data.headers()),
        body(http_data.Body()),
        timestamp(rhs.timestamp) {}

  // Support objects with user-defined HTTP response handlers.
  template <typename T>
  inline typename std::enable_if<HasRespondViaHTTP<current::decay<T>>(0)>::type operator()(
      T&& that_dude_over_there) {
    that_dude_over_there.RespondViaHTTP(std::move(*this));
  }

  // A shortcut to allow `[](Request r) { r("OK"); }` instead of `r.connection.SendHTTPResponse("OK")`.
  template <typename... TS>
  void operator()(TS&&... params) {
    connection.SendHTTPResponse(std::forward<TS>(params)...);
  }

  current::net::HTTPServerConnection::ChunkedResponseSender SendChunkedResponse() {
    return connection.SendChunkedHTTPResponse();
  }

  Request(const Request&) = delete;
  void operator=(const Request&) = delete;
  void operator=(Request&&) = delete;
};

}  // namespace blocks

#endif  // BLOCKS_HTTP_REQUEST_H
