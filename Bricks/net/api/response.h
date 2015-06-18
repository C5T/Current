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

#ifndef BRICKS_NET_API_RESPONSE_H
#define BRICKS_NET_API_RESPONSE_H

// A convenience wrapper to pass in to `Request::operator()`, for the cases where
// the best design is to return a value from one function to have the next one in the chain,
// which can be `std::move(request)`, to pick it up.

#include "request.h"

#include "../http/http.h"
#include "../../strings/is_string_type.h"
#include "../../cerealize/cerealize.h"

namespace bricks {
namespace net {
namespace api {

static_assert(strings::is_string_type<const char*>::value, "");
static_assert(!strings::is_string_type<int>::value, "");

struct Response {
  std::string body;
  HTTPResponseCodeValue code;
  std::string content_type;
  HTTPHeadersType extra_headers;

  Response() = delete;
  void operator=(const Response&) = delete;
  void operator=(Response&&) = delete;

  template <typename... ARGS>
  Response(ARGS&&... args) {
    Construct(std::forward<ARGS>(args)...);
  }

  void Construct(const Response& rhs) {
    body = rhs.body;
    code = rhs.code;
    content_type = rhs.content_type;
    extra_headers = rhs.extra_headers;
  }

  void Construct(Response&& rhs) {
    body = std::move(rhs.body);
    code = rhs.code;
    content_type = rhs.content_type;
    extra_headers = rhs.extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          strings::is_string_type<T>::value>::type
  Construct(T&& body,
            HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const std::string& content_type = HTTPServerConnection::DefaultContentType(),
            const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    this->body = std::forward<T>(body);
    this->code = code;
    this->content_type = content_type;
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          !(strings::is_string_type<T>::value)>::type
  Construct(T&& object,
            HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    this->body = cerealize::JSON(std::forward<T>(object));
    this->code = code;
    this->content_type = HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          !(strings::is_string_type<T>::value)>::type
  Construct(T&& object,
            const std::string& object_name,
            HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const HTTPHeadersType& extra_headers = HTTPHeadersType()) {
    this->body = cerealize::JSON(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  Response& Body(const std::string& s) {
    body = s;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object) {
    body = cerealize::JSON(object);
    content_type = HTTPServerConnection::DefaultJSONContentType();
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object, const std::string& object_name) {
    body = cerealize::JSON(object, object_name);
    content_type = HTTPServerConnection::DefaultJSONContentType();
    return *this;
  }

  Response& Code(HTTPResponseCodeValue c) {
    code = c;
    return *this;
  }

  Response& ContentType(const std::string& s) {
    content_type = s;
    return *this;
  }

  Response& Headers(const HTTPHeadersType& h) {
    extra_headers = h;
    return *this;
  }

  void RespondViaHTTP(Request r) const { r(body, code, content_type, extra_headers); }
};

static_assert(Request::HasRespondViaHTTP<Response>::value, "");

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_RESPONSE_H
