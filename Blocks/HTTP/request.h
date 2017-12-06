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

#include <vector>
#include <string>

#include "../URL/url.h"

#include "../../Bricks/net/http/http.h"
#include "../../Bricks/time/chrono.h"
#include "../../Bricks/template/decay.h"

namespace current {
namespace http {

struct Request;

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
  const current::url::URL url;
  // `url_path_had_trailing_slash` is needed to distinguish requests with and without a trailing slash
  // and redirect the latter to the former.
  // Do not store the original `url` to avoid taking extra memory.
  const bool url_path_had_trailing_slash;
  const current::url::URLPathArgs url_path_args;
  const std::string method;
  const current::net::http::Headers& headers;
  const std::string& body;  // TODO(dkorolev): This is inefficient, but will do.
  const std::chrono::microseconds timestamp;

  explicit Request(std::unique_ptr<current::net::HTTPServerConnection>&& connection,
                   current::url::URLPathArgs url_path_args = current::url::URLPathArgs())
      : unique_connection(std::move(connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(http_data.URL()),
        url_path_had_trailing_slash(!url.path.empty() && url.path.back() == '/'),
        url_path_args(url_path_args),
        method(http_data.Method()),
        headers(http_data.headers()),
        body(http_data.Body()),
        timestamp(current::time::Now()) {
    // Adjust the URL path to match the path of the handler:
    // * Remove URL argument.
    //   (when calling a { "/foo" with one URL arg } handler, url.path will be "/foo" for a "/foo/x" request).
    // * Remove trailing slashes, if any.
    url.path.resize(url_path_args.base_path.length());
  }

  // It is essential to move `unique_connection` so that the socket outlives the destruction of `rhs`.
  Request(Request&& rhs)
      : unique_connection(std::move(rhs.unique_connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(rhs.url),
        url_path_had_trailing_slash(rhs.url_path_had_trailing_slash),
        url_path_args(rhs.url_path_args),
        method(http_data.Method()),
        headers(http_data.headers()),
        body(http_data.Body()),
        timestamp(rhs.timestamp) {}

  // Support objects with user-defined HTTP response handlers.
  template <typename T>
  inline typename std::enable_if<HasRespondViaHTTP<current::decay<T>>(0)>::type operator()(T&& that_dude_over_there) {
    if (!unique_connection) {
      CURRENT_THROW(net::AttemptedToSendHTTPResponseMoreThanOnce());
    }
    that_dude_over_there.RespondViaHTTP(std::move(*this));
  }

  // A shortcut to allow `[](Request r) { r("OK"); }` instead of `r.connection.SendHTTPResponse("OK")`.
  template <typename... TS>
  void operator()(TS&&... params) {
    if (!unique_connection) {
      CURRENT_THROW(net::AttemptedToSendHTTPResponseMoreThanOnce());
    }
    connection.SendHTTPResponse(std::forward<TS>(params)...);
  }

  current::net::HTTPServerConnection::ChunkedResponseSender SendChunkedResponse(
      net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
      const std::string& content_type = net::constants::kDefaultJSONContentType,
      const net::http::Headers& extra_headers = net::http::Headers::DefaultJSONHeaders()) {
    if (!unique_connection) {
      CURRENT_THROW(net::AttemptedToSendHTTPResponseMoreThanOnce());
    }
    return connection.SendChunkedHTTPResponse(code, content_type, extra_headers);
  }

  Request(const Request&) = delete;
  void operator=(const Request&) = delete;
  void operator=(Request&&) = delete;
};

}  // namespace http
}  // namespace current

#endif  // BLOCKS_HTTP_REQUEST_H
