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

// Please refer to the header comment in `api.h` for the details.
// This `types.h` file is used to remove cyclic dependency between impl/{posix,apple,etc}.h and api.h.

#ifndef BLOCKS_HTTP_TYPES_H
#define BLOCKS_HTTP_TYPES_H

#include <mutex>
#include <memory>
#include <map>
#include <string>

#include "../../Bricks/net/exceptions.h"
#include "../../Bricks/net/http/codes.h"
#include "../../Bricks/strings/is_string_type.h"
#include "../../Bricks/cerealize/cerealize.h"

namespace blocks {

// Structures to define HTTP requests.
// Support GET and POST.
// The syntax for creating an instance of a GET request is GET is `GET(url)`.
// The syntax for creating an instance of a POST request is POST is `POST(url, data, content_type)`'.
// Alternatively, `POSTFromFile(url, file_name, content_type)` is supported.
// Both GET and two forms of POST allow `.UserAgent(custom_user_agent)`.
template <typename T>
struct HTTPRequestBase {
  std::string url;
  std::string custom_user_agent = "";
  bool allow_redirects = false;

  HTTPRequestBase(const std::string& url) : url(url) {}

  T& UserAgent(const std::string& new_custom_user_agent) {
    custom_user_agent = new_custom_user_agent;
    return static_cast<T&>(*this);
  }

  T& AllowRedirects(bool allow_redirects_setting = true) {
    allow_redirects = allow_redirects_setting;
    return static_cast<T&>(*this);
  }
};

struct GET : HTTPRequestBase<GET> {
  explicit GET(const std::string& url) : HTTPRequestBase(url) {}
};

// A helper class to fill in request body as either plain text of JSON-ified object.
template <typename REQUEST, bool IS_JSON>
struct FillBody {};

// Body as string, default the type to "text/plain", when `is_string_type<T>::value == true`.
template <typename REQUEST>
struct FillBody<REQUEST, true> {
  static void Fill(REQUEST& request, const std::string& body, const std::string& content_type) {
    request.body = body;
    request.content_type = !content_type.empty() ? content_type : "text/plain";
  }
};

// Body as JSON, default the type to "application/json", when `is_string_type<T>::value == false`.
template <typename REQUEST>
struct FillBody<REQUEST, false> {
  template <typename T>
  static void Fill(REQUEST& request, T&& object, const std::string& content_type) {
    static_assert(bricks::cerealize::is_write_cerealizable<T>::value,
                  "This form of POST() requires a cerealizable object as the second parameter.");
    request.body = bricks::cerealize::JSON(object, "data");
    request.content_type = !content_type.empty() ? content_type : "application/json";
  }
};

struct POST : HTTPRequestBase<POST> {
  std::string body;
  std::string content_type;

  template <typename T>
  POST(const std::string& url, T&& body, const std::string& content_type = "")
      : HTTPRequestBase(url) {
    FillBody<POST, bricks::strings::is_string_type<T>::value>::Fill(*this, body, content_type);
  }
};

struct POSTFromFile : HTTPRequestBase<POSTFromFile> {
  std::string file_name;
  std::string content_type;

  explicit POSTFromFile(const std::string& url, const std::string& file_name, const std::string& content_type)
      : HTTPRequestBase(url), file_name(file_name), content_type(content_type) {}
};

struct PUT : HTTPRequestBase<PUT> {
  std::string body;
  std::string content_type;

  template <typename T>
  PUT(const std::string& url, T&& body, const std::string& content_type = "")
      : HTTPRequestBase(url) {
    FillBody<PUT, bricks::strings::is_string_type<T>::value>::Fill(*this, body, content_type);
  }
};

struct DELETE : HTTPRequestBase<DELETE> {
  explicit DELETE(const std::string& url) : HTTPRequestBase(url) {}
};

// Structures to define HTTP response.
// The actual response type is templated and depends on the types of input paramteres.
// All responses inherit from `struct HTTPResponse`, that lists common fields.
struct HTTPResponse {
  // The final URL. Will be equal to the original URL, unless redirects have been allowed and took place.
  std::string url;
  // HTTP response code.
  bricks::net::HTTPResponseCodeValue code;
};

struct HTTPResponseWithBuffer : HTTPResponse {
  std::string body;  // Returned HTTP body, saved as an in-memory buffer, stored in std::string.
};

struct HTTPResponseWithResultingFileName : HTTPResponse {
  std::string body_file_name;  // The file name into which the returned HTTP body has been saved.
};

// Response storage policy.
// The default one is `KeepResponseInMemory()`, which can be omitted.
// The alternative one is `SaveResponseToFile(file_name)`.
struct KeepResponseInMemory {};

struct SaveResponseToFile {
  std::string file_name;
  explicit SaveResponseToFile(const std::string& file_name) : file_name(file_name) {}
};

// Template metaprogramming for selecting the right types at compile time,
// taking into account the types of request and response.
template <typename T>
struct ResponseTypeFromRequestType {};

template <>
struct ResponseTypeFromRequestType<KeepResponseInMemory> {
  typedef HTTPResponseWithBuffer T_RESPONSE_TYPE;
};

template <>
struct ResponseTypeFromRequestType<SaveResponseToFile> {
  typedef HTTPResponseWithResultingFileName T_RESPONSE_TYPE;
};

// To allow adding new implementation to the framework defined in api.h, the corresponding implementations
// should be added as `class FooImpl { ... }; template<> class ImplWrapper<FooImpl> { ... }`.
template <class T>
class ImplWrapper {};

// The main implementation of what `HTTP` actually is.
// The real work is done by templated implementations.
template <typename CLIENT_IMPL, class SERVER_IMPL>
struct HTTPImpl {
  typedef CLIENT_IMPL T_CLIENT_IMPL;
  typedef SERVER_IMPL T_SERVER_IMPL;

  T_SERVER_IMPL& operator()(int port) {
    static std::mutex mutex;
    static std::map<size_t, std::unique_ptr<T_SERVER_IMPL>> servers;
    std::lock_guard<std::mutex> lock(mutex);
    std::unique_ptr<T_SERVER_IMPL>& server = servers[port];
    if (!server) {
      server.reset(new T_SERVER_IMPL(port));
    }
    return *server;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS = KeepResponseInMemory>
  inline typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE operator()(
      const T_REQUEST_PARAMS& request_params,
      const T_RESPONSE_PARAMS& response_params = T_RESPONSE_PARAMS()) const {
    T_CLIENT_IMPL impl;
    typedef ImplWrapper<T_CLIENT_IMPL> IMPL_HELPER;
    IMPL_HELPER::PrepareInput(request_params, impl);
    IMPL_HELPER::PrepareInput(response_params, impl);
    if (!impl.Go()) {
#ifndef ANDROID
      BRICKS_THROW(bricks::net::HTTPException());  // LCOV_EXCL_LINE
#else
      // TODO(dkorolev): Chat with Alex. We can overcome the exception here, but should we?
      return typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE();
#endif
    }
    typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE output;
    IMPL_HELPER::ParseOutput(request_params, response_params, impl, output);
    return output;
  }
};

}  // namespace blocks

using blocks::GET;
using blocks::POST;
using blocks::POSTFromFile;
using blocks::PUT;
using blocks::DELETE;

#endif  // BLOCKS_HTTP_TYPES_H
