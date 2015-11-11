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

#ifndef BLOCKS_HTTP_RESPONSE_H
#define BLOCKS_HTTP_RESPONSE_H

// A convenience wrapper to pass in to `Request::operator()`, for the cases where
// the best design is to return a value from one function to have the next one in the chain,
// which can be `std::move(request)`, to pick it up.

#include "request.h"

#include "../../TypeSystem/struct.h"

#include "../../Bricks/net/exceptions.h"
#include "../../Bricks/net/http/http.h"
#include "../../Bricks/strings/is_string_type.h"
#include "../../Bricks/cerealize/cerealize.h"

namespace blocks {

struct Response {
  bool initialized = false;

  std::string body;
  bricks::net::HTTPResponseCodeValue code;
  std::string content_type;
  bricks::net::HTTPHeadersType extra_headers;

  Response()
      : body(""),
        code(HTTPResponseCode.OK),
        content_type(bricks::net::HTTPServerConnection::DefaultContentType()),
        extra_headers(bricks::net::HTTPHeadersType()) {}

  Response& operator=(const Response&) = default;
  Response& operator=(Response&&) = default;

  template <typename... ARGS>
  Response(ARGS&&... args)
      : initialized(true) {
    Construct(std::forward<ARGS>(args)...);
  }

  void Construct(const Response& rhs) {
    initialized = rhs.initialized;
    body = rhs.body;
    code = rhs.code;
    content_type = rhs.content_type;
    extra_headers = rhs.extra_headers;
  }

  void Construct(Response&& rhs) {
    initialized = rhs.initialized;
    body = std::move(rhs.body);
    code = rhs.code;
    content_type = rhs.content_type;
    extra_headers = rhs.extra_headers;
  }

  void Construct(bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
                 const std::string& content_type = bricks::net::HTTPServerConnection::DefaultContentType(),
                 const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = "";
    this->code = code;
    this->content_type = content_type;
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          bricks::strings::is_string_type<T>::value>::type
  Construct(T&& body,
            bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const std::string& content_type = bricks::net::HTTPServerConnection::DefaultContentType(),
            const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = std::forward<T>(body);
    this->code = code;
    this->content_type = content_type;
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          !(bricks::strings::is_string_type<T>::value) &&
                          !std::is_base_of<current::reflection::CurrentSuper, T>::value>::type
  Construct(T&& object,
            bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = CerealizeJSON(std::forward<T>(object)) + '\n';
    this->code = code;
    this->content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<!std::is_same<bricks::decay<T>, Response>::value &&
                          !(bricks::strings::is_string_type<T>::value) &&
                          !std::is_base_of<current::reflection::CurrentSuper, T>::value>::type
  Construct(T&& object,
            const std::string& object_name,
            bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
            const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = CerealizeJSON(std::forward<T>(object), object_name) + '\n';
    this->code = code;
    this->content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<std::is_base_of<current::reflection::CurrentSuper, T>::value>::type Construct(
      T&& object,
      bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
      const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = current::serialization::JSON(std::forward<T>(object)) + '\n';
    this->code = code;
    this->content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  typename std::enable_if<std::is_base_of<current::reflection::CurrentSuper, T>::value>::type Construct(
      T&& object,
      const std::string& object_name,
      bricks::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
      const bricks::net::HTTPHeadersType& extra_headers = bricks::net::HTTPHeadersType()) {
    this->body = "{\"" + object_name + "\":" + current::serialization::JSON(std::forward<T>(object)) + "}\n";
    this->code = code;
    this->content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    this->extra_headers = extra_headers;
  }

  Response& Body(const std::string& s) {
    body = s;
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& CerealizableJSON(const T& object) {
    body = CerealizeJSON(object) + '\n';
    content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& CerealizableJSON(const T& object, const std::string& object_name) {
    body = CerealizeJSON(object, object_name) + '\n';
    content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object) {
    body = current::serialization::JSON(object) + '\n';
    content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object, const std::string& object_name) {
    body = "{\"" + object_name + "\":" + current::serialization::JSON(object) + "}\n";
    content_type = bricks::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  Response& Code(bricks::net::HTTPResponseCodeValue c) {
    code = c;
    initialized = true;
    return *this;
  }

  Response& ContentType(const std::string& s) {
    content_type = s;
    initialized = true;
    return *this;
  }

  Response& Headers(const bricks::net::HTTPHeadersType& h) {
    extra_headers = h;
    initialized = true;
    return *this;
  }

  void RespondViaHTTP(Request r) const {
    if (initialized) {
      r(body, code, content_type, extra_headers);
    }
    // Else, a 500 "INTERNAL SERVER ERROR" will be returned, since `Request`
    // has not been served upon destructing at the exit from this method.
  }
};

static_assert(HasRespondViaHTTP<Response>(0), "");

}  // namespace blocks

#endif  // BLOCKS_HTTP_RESPONSE_H
