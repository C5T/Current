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

#include "../../typesystem/struct.h"

#include "../../bricks/net/exceptions.h"
#include "../../bricks/net/http/http.h"
#include "../../bricks/strings/is_string_type.h"

namespace current {
namespace http {

namespace impl {
template <bool IS_STRING>
struct FillResponseHelper;

template <>
struct FillResponseHelper<true> {
  template <typename T>
  static std::string AsString(T&& object) {
    return std::string(std::forward<T>(object));
  }
  static std::string DefaultContentType() { return net::constants::kDefaultContentType; }
};

template <>
struct FillResponseHelper<false> {
  template <typename T>
  static std::string AsString(T&& object) {
    return JSON(std::forward<T>(object)) + '\n';
  }
  template <typename T>
  static std::string AsString(T&& object, const std::string& object_name) {
    return "{\"" + object_name + "\":" + JSON(std::forward<T>(object)) + "}\n";
  }
  static std::string DefaultContentType() { return net::constants::kDefaultJSONContentType; }
};
}  // namespace impl

struct Response final : IHasDoRespondViaHTTP {
  bool initialized = false;

  std::string body;
  net::HTTPResponseCodeValue code;
  std::string content_type;
  net::http::Headers headers;
  bool cors_header = true;

  Response() : body(""), code(HTTPResponseCode.OK), content_type(net::constants::kDefaultContentType) {}

  Response(const Response&) = default;
  Response(Response&) = default;  // Apparently, required to return a dot-constructed `Response` from a function.
  Response(Response&&) = default;

  Response& operator=(const Response&) = default;
  Response& operator=(Response&) = default;
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
    headers = rhs.headers;
  }

  void Construct(Response&& rhs) {
    initialized = rhs.initialized;
    body = std::move(rhs.body);
    code = rhs.code;
    content_type = rhs.content_type;
    headers = rhs.headers;
  }

  void Construct(net::HTTPResponseCodeValue code = HTTPResponseCode.OK,
                 const std::string& content_type = net::constants::kDefaultContentType,
                 const net::http::Headers& headers = net::http::Headers()) {
    this->body = "";
    this->code = code;
    this->content_type = content_type;
    this->headers = headers;
  }

  template <typename T>
  void Construct(T&& object, net::HTTPResponseCodeValue code = HTTPResponseCode.OK) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = G::DefaultContentType();
  }

  template <typename T>
  void Construct(T&& object, const std::string& object_name, net::HTTPResponseCodeValue code = HTTPResponseCode.OK) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = G::DefaultContentType();
  }

  template <typename T>
  void Construct(T&& object, net::HTTPResponseCodeValue code, const net::http::Headers& headers) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = G::DefaultContentType();
    this->headers = headers;
  }

  template <typename T>
  void Construct(T&& object,
                 const std::string& object_name,
                 net::HTTPResponseCodeValue code,
                 const net::http::Headers& headers) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = G::DefaultContentType();
    this->headers = headers;
  }

  template <typename T>
  void Construct(T&& object,
                 net::HTTPResponseCodeValue code,
                 const std::string& content_type,
                 const net::http::Headers& headers = net::http::Headers()) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object));
    this->code = code;
    this->content_type = content_type;
    this->headers = headers;
  }

  template <typename T>
  void Construct(T&& object,
                 const std::string& object_name,
                 net::HTTPResponseCodeValue code,
                 const std::string& content_type,
                 const net::http::Headers& headers = net::http::Headers()) {
    using G = impl::FillResponseHelper<strings::is_string_type<T>::value>;
    this->body = G::AsString(std::forward<T>(object), object_name);
    this->code = code;
    this->content_type = content_type;
    this->headers = headers;
  }

  Response& Body(const std::string& s) {
    body = s;
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object) {
    body = current::JSON(object) + '\n';
    content_type = net::constants::kDefaultJSONContentType;
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& JSON(const T& object, const std::string& object_name) {
    body = "{\"" + object_name + "\":" + current::JSON(object) + "}\n";
    content_type = net::constants::kDefaultJSONContentType;
    initialized = true;
    return *this;
  }

  Response& Code(net::HTTPResponseCodeValue c) {
    code = c;
    initialized = true;
    return *this;
  }

  Response& ContentType(const std::string& s) {
    content_type = s;
    initialized = true;
    return *this;
  }

  Response& Headers(const net::http::Headers& h) {
    headers = h;
    initialized = true;
    return *this;
  }

  Response& SetHeader(const std::string& key, const std::string& value) {
    headers[key] = value;
    initialized = true;
    return *this;
  }

  template <typename T>
  Response& SetCookie(const std::string& name, T&& cookie) {
    headers.SetCookie(name, std::forward<T>(cookie));
    initialized = true;
    return *this;
  }

  Response& SetCookie(const std::string& name,
                      const std::string& cookie,
                      const std::map<std::string, std::string>& params) {
    headers.SetCookie(name, cookie, params);
    initialized = true;
    return *this;
  }

  Response& DisableCORS() {
    cors_header = false;
    return *this;
  }

  Response& EnableCORS() {
    cors_header = true;
    return *this;
  }

  void DoRespondViaHTTP(Request r) const override {
    if (initialized) {
      if (cors_header) {
        auto mutable_headers = headers;
        mutable_headers.SetCORSHeader();
        r(body, code, mutable_headers, content_type);
      } else {
        r(body, code, headers, content_type);
      }
    }
    // Else, a 500 "INTERNAL SERVER ERROR" will be returned, since `Request`
    // has not been served upon destructing at the exit from this method.
  }
};

template <typename T, bool CAN_JSON>
struct GenerateResponseFromMaybeSerializableObjectImpl;

template <typename T>
struct GenerateResponseFromMaybeSerializableObjectImpl<T, true> {
  template <typename V>
  static Response DoIt(V&& value, current::net::HTTPResponseCodeValue code) {
    return Response(value, code);
  }
};

template <typename T>
struct GenerateResponseFromMaybeSerializableObjectImpl<T, false> {
  template <typename V>
  static Response DoIt(V&& value, current::net::HTTPResponseCodeValue code) {
    static_cast<void>(value);
    return Response("", code);
  }
};

template <typename T>
inline Response GenerateResponseFromMaybeSerializableObject(
    T&& value, current::net::HTTPResponseCodeValue code = HTTPResponseCode.OK) {
  return GenerateResponseFromMaybeSerializableObjectImpl<
      T,
      current::serialization::json::IsJSONSerializable<T>::value>::DoIt(value, code);
}

}  // namespace http
}  // namespace current

#endif  // BLOCKS_HTTP_RESPONSE_H
