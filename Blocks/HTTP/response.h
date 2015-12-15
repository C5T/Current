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

template <bool IS_STRING, bool IS_CURRENT_STRUCT>
struct StringBodyGenerator;

template <>
struct StringBodyGenerator<true, false> {
  template <typename T>
  static std::string AsString(T&& object) {
    return object;
  }
  static std::string DefaultContentType() { return current::net::HTTPServerConnection::DefaultContentType(); }
};

template <>
struct StringBodyGenerator<false, true> {
  template <typename T>
  static std::string AsString(T&& object) {
    return current::serialization::JSON(std::forward<T>(object)) + '\n';
  }
  template <typename T>
  static std::string AsString(T&& object, const std::string& object_name) {
    return "{\"" + object_name + "\":" + current::serialization::JSON(std::forward<T>(object)) + "}\n";
  }
  static std::string DefaultContentType() {
    return current::net::HTTPServerConnection::DefaultJSONContentType();
  }
};

template <>
struct StringBodyGenerator<false, false> {
  template <typename T>
  static std::string AsString(T&& object) {
    return CerealizeJSON(std::forward<T>(object)) + '\n';
  }
  template <typename T>
  static std::string AsString(T&& object, const std::string& object_name) {
    return CerealizeJSON(std::forward<T>(object), object_name) + '\n';
  }
  static std::string DefaultContentType() {
    return current::net::HTTPServerConnection::DefaultJSONContentType();
  }
};

struct Response {
  bool initialized = false;

  std::string body;
  current::net::HTTPResponseCodeValue code;
  std::string content_type;
  current::net::HTTPHeadersType extra_headers;

  Response()
      : body(""),
        code(HTTPResponseCode.OK),
        content_type(current::net::HTTPServerConnection::DefaultContentType()),
        extra_headers(current::net::HTTPHeadersType()) {}

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

  void Construct(current::net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
                 const std::string& content_type = current::net::HTTPServerConnection::DefaultContentType(),
                 const current::net::HTTPHeadersType& extra_headers = current::net::HTTPHeadersType()) {
    this->body = "";
    this->code = code;
    this->content_type = content_type;
    this->extra_headers = extra_headers;
  }

  template <typename T>
  void Construct(T&& object, current::net::HTTPResponseCodeValue code = HTTPResponseCode.OK) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = G::DefaultContentType();
  }

  template <typename T>
  void Construct(T&& object,
                 const std::string& object_name,
                 current::net::HTTPResponseCodeValue code = HTTPResponseCode.OK) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = G::DefaultContentType();
  }

  template <typename T>
  void Construct(T&& object,
                 current::net::HTTPResponseCodeValue code,
                 const current::net::HTTPHeadersType& extra_headers) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = G::DefaultContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  void Construct(T&& object,
                 const std::string& object_name,
                 current::net::HTTPResponseCodeValue code,
                 const current::net::HTTPHeadersType& extra_headers) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = G::DefaultContentType();
    this->extra_headers = extra_headers;
  }

  template <typename T>
  void Construct(T&& object,
                 current::net::HTTPResponseCodeValue code,
                 const std::string& content_type,
                 const current::net::HTTPHeadersType& extra_headers = current::net::HTTPHeadersType()) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = content_type;
    this->extra_headers = extra_headers;
  }

  template <typename T>
  void Construct(T&& object,
                 const std::string& object_name,
                 current::net::HTTPResponseCodeValue code,
                 const std::string& content_type,
                 const current::net::HTTPHeadersType& extra_headers = current::net::HTTPHeadersType()) {
    using G = StringBodyGenerator<current::strings::is_string_type<T>::value, IS_CURRENT_STRUCT(T)>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = content_type;
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
    content_type = current::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& CerealizableJSON(const T& object, const std::string& object_name) {
    body = CerealizeJSON(object, object_name) + '\n';
    content_type = current::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object) {
    body = current::serialization::JSON(object) + '\n';
    content_type = current::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object, const std::string& object_name) {
    body = "{\"" + object_name + "\":" + current::serialization::JSON(object) + "}\n";
    content_type = current::net::HTTPServerConnection::DefaultJSONContentType();
    initialized = true;
    return *this;
  }

  Response& Code(current::net::HTTPResponseCodeValue c) {
    code = c;
    initialized = true;
    return *this;
  }

  Response& ContentType(const std::string& s) {
    content_type = s;
    initialized = true;
    return *this;
  }

  Response& Headers(const current::net::HTTPHeadersType& h) {
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
