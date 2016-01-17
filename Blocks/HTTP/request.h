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

struct URLPathArgs {
 public:
  using CountMaskUnderlyingType = uint16_t;
  enum class CountMask : CountMaskUnderlyingType {
    None = (1 << 0),
    One = (1 << 1),
    Two = (1 << 2),
    Three = (1 << 3),
    Any = static_cast<CountMaskUnderlyingType>(~0)
  };

  const std::string& operator[](size_t index) const { return args_.at(args_.size() - 1 - index); }

  using T_ITERATOR = std::vector<std::string>::const_reverse_iterator;
  T_ITERATOR begin() const { return args_.crbegin(); }
  T_ITERATOR end() const { return args_.crend(); }

  bool empty() const { return args_.empty(); }
  size_t size() const { return args_.size(); }

  void add(const std::string& arg) { args_.push_back(arg); }

  std::string base_path;

 private:
  std::vector<std::string> args_;
};

inline URLPathArgs::CountMask operator&(const URLPathArgs::CountMask lhs, const URLPathArgs::CountMask rhs) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) &
                                             static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
}

inline URLPathArgs::CountMask operator|(const URLPathArgs::CountMask lhs, const URLPathArgs::CountMask rhs) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) |
                                             static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
}

inline URLPathArgs::CountMask& operator|=(URLPathArgs::CountMask& lhs, const URLPathArgs::CountMask rhs) {
  lhs = static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) |
                                            static_cast<URLPathArgs::CountMaskUnderlyingType>(rhs));
  return lhs;
}

inline URLPathArgs::CountMask operator<<(const URLPathArgs::CountMask lhs, const size_t count) {
  return static_cast<URLPathArgs::CountMask>(static_cast<URLPathArgs::CountMaskUnderlyingType>(lhs) << count);
}

inline URLPathArgs::CountMask& operator<<=(URLPathArgs::CountMask& lhs, const size_t count) {
  lhs = (lhs << count);
  return lhs;
}

// The only parameter to be passed to HTTP handlers.
struct Request final {
  std::unique_ptr<current::net::HTTPServerConnection> unique_connection;

  current::net::HTTPServerConnection& connection;
  const current::net::HTTPRequestData&
      http_data;  // Accessor to use `r.http_data` instead of `r.connection->HTTPRequest()`.
  const URL url;
  const URLPathArgs url_path_args;
  const std::string method;
  const current::net::HTTPRequestData::HeadersType& headers;
  const std::string& body;  // TODO(dkorolev): This is inefficient, but will do.
  const std::chrono::microseconds timestamp;

  explicit Request(std::unique_ptr<current::net::HTTPServerConnection>&& connection,
                   URLPathArgs url_path_args = URLPathArgs())
      : unique_connection(std::move(connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(http_data.URL()),
        url_path_args(url_path_args),
        method(http_data.Method()),
        headers(http_data.headers()),
        body(http_data.Body()),
        timestamp(current::time::Now()) {
    if (!url_path_args.empty()) {
      url.path.resize(url_path_args.base_path.length());
    }
  }

  // It is essential to move `unique_connection` so that the socket outlives the destruction of `rhs`.
  Request(Request&& rhs)
      : unique_connection(std::move(rhs.unique_connection)),
        connection(*unique_connection.get()),
        http_data(unique_connection->HTTPRequest()),
        url(rhs.url),
        url_path_args(rhs.url_path_args),
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
