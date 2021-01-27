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
#include <functional>

#include "../../bricks/net/exceptions.h"
#include "../../bricks/net/http/http.h"
#include "../../bricks/net/http/impl/server.h"  // net::constants
#include "../../bricks/strings/is_string_type.h"
#include "../../bricks/template/decay.h"

namespace current {
namespace http {

// The policy for registering HTTP endpoints.
// TODO(dkorolev): Add another option, to throw if the handler does not exist, while it's expected to?
enum class ReRegisterRoute { ThrowOnAttempt, SilentlyUpdateExisting };

// Structures to define HTTP requests.
// The syntax for creating an instance of a GET request is GET is `GET(url)`.
// The syntax for creating an instance of a POST request is POST is `POST(url, data, content_type)`'.
// Alternatively, `POSTFromFile(url, file_name, content_type)` is supported.
// Both GET and two forms of POST allow `.UserAgent(custom_user_agent)`.
// Addendum 9/24/2016: We have HEAD, DELETE, and PUT since a while ago, and I've just added PATCH. -- D.K.
template <typename T>
struct HTTPRequestBase {
  std::string url;
  std::string custom_user_agent = "";
  current::net::http::Headers custom_headers;
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

  T& SetHeader(const std::string& key, const std::string& value) {
    custom_headers.emplace_back(key, value);
    return static_cast<T&>(*this);
  }

  // Note: Client-side cookie, no params here. -- @mzhurovich, @dkorolev.
  T& SetCookie(const std::string& cookie, const std::string& value) {
    custom_headers.SetCookie(cookie, value);
    return static_cast<T&>(*this);
  }
};

struct GET : HTTPRequestBase<GET> {
  explicit GET(const std::string& url) : HTTPRequestBase(url) {}
};

template <typename T>
struct ChunkedBase : HTTPRequestBase<T> {
  using super_t = HTTPRequestBase<T>;

  const std::function<void(const std::string&, const std::string&, current::net::HTTPResponseKind&)> header_callback;
  const std::function<void(const std::string&)> chunk_callback;
  const std::function<void()> done_callback;

  std::vector<std::function<void(const std::string&)>> line_callbacks_;
  std::unique_ptr<current::strings::StatefulGroupByLines> lines_builder_;

  std::function<void(const std::string&, const std::string&, current::net::HTTPResponseKind&)> header_callback_impl;
  std::function<void(const std::string&)> chunk_callback_impl;
  std::function<void()> done_callback_impl;

  explicit ChunkedBase(std::string url)
    : super_t(std::move(url)),
      header_callback([this](const std::string& k, const std::string& v, current::net::HTTPResponseKind& rk) {
        header_callback_wrapper(k, v, rk);
      }),
      chunk_callback([this](const std::string& c) { chunk_callback_wrapper(c); }),
      done_callback([this]() { done_callback_wrapper(); }) {}

  explicit ChunkedBase(std::string url,
                       std::function<void(const std::string&, const std::string&)> header_callback,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void()> done_callback = []() {})
      : ChunkedBase(std::move(url)) {
    header_callback_impl = [header_callback](const std::string& k,
                                             const std::string& v,
                                             current::net::HTTPResponseKind&) {
      header_callback(k, v);
    };
    chunk_callback_impl = chunk_callback;
    done_callback_impl = done_callback;
  }

  explicit ChunkedBase(std::string url,
                       std::function<void(const std::string&,
                                          const std::string&,
                                          current::net::HTTPResponseKind&)> header_callback,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void()> done_callback = []() {})
      : ChunkedBase(std::move(url)) {
    header_callback_impl = header_callback;
    chunk_callback_impl = chunk_callback;
    done_callback_impl = done_callback;
  }

  T& OnHeader(std::function<void(const std::string&, const std::string&)> header_callback) {
    header_callback_impl = [header_callback](const std::string& k,
                                             const std::string& v,
                                             current::net::HTTPResponseKind&) {
      header_callback(k, v);
    };
    return static_cast<T&>(*this);
  }
  T& OnHeader(std::function<void(const std::string&,
                                 const std::string&,
                                 current::net::HTTPResponseKind&)> header_callback) {
    header_callback_impl = header_callback;
    return static_cast<T&>(*this);
  }
  T& OnChunk(std::function<void(const std::string&)> chunk_callback) {
    chunk_callback_impl = chunk_callback;
    return static_cast<T&>(*this);
  }
  T& OnDone(std::function<void()> done_callback) {
    done_callback_impl = done_callback;
    return static_cast<T&>(*this);
  }

  // TODO(dkorolev): Move this to `Chunk`-s if performance becomes the bottleneck.
  T& OnLine(std::function<void(const std::string&)> line_callback) {
    if (!lines_builder_) {
      lines_builder_ = std::make_unique<current::strings::StatefulGroupByLines>([this](const std::string& line) {
        for (const std::function<void(const std::string& line)>& cb : line_callbacks_) {
          cb(line);
        }
      });
    }
    line_callbacks_.push_back(line_callback);
    return static_cast<T&>(*this);
  }

  void header_callback_wrapper(const std::string& k, const std::string& v, current::net::HTTPResponseKind& rk) {
    if (header_callback_impl) {
      header_callback_impl(k, v, rk);
    }
  }

  void chunk_callback_wrapper(const std::string& c) {
    if (lines_builder_) {
      lines_builder_->Feed(c);
    }
    if (chunk_callback_impl) {
      chunk_callback_impl(c);
    }
  }

  void done_callback_wrapper() {
    lines_builder_ = nullptr;
    if (done_callback_impl) {
      done_callback_impl();
    }
  }
};

struct ChunkedGET final : ChunkedBase<ChunkedGET> {
  using super_t = ChunkedBase<ChunkedGET>;
  using super_t::super_t;
};

struct ChunkedPOST final : ChunkedBase<ChunkedPOST> {
  using super_t = ChunkedBase<ChunkedPOST>;
  using super_t::super_t;
  const std::string body;
  const std::string content_type;
  explicit ChunkedPOST(std::string url,
                       std::string body,
                       std::string content_type,
                       std::function<void(const std::string&, const std::string&)> header_callback,
                       std::function<void(const std::string&)> chunk_callback,
                       std::function<void()> done_callback = []() {})
      : ChunkedBase(std::move(url), header_callback, chunk_callback, done_callback),
        body(std::move(body)),
        content_type(std::move(content_type)) {}
  explicit ChunkedPOST(std::string url, std::string body, std::string content_type = "")
      : ChunkedBase(std::move(url)), body(std::move(body)), content_type(std::move(content_type)) {}
};

struct HEAD : HTTPRequestBase<HEAD> {
  explicit HEAD(const std::string& url) : HTTPRequestBase(url) {}
};

// A helper class to fill in request body as either plain text or JSON-ified object.
template <typename REQUEST, bool IS_STRING_TYPE>
struct FillBody {};

// Body as string, default the type to "text/plain", when `is_string_type<T>::value == true`.
template <typename REQUEST>
struct FillBody<REQUEST, true> {
  static void Fill(REQUEST& request, const std::string& body, const std::string& content_type) {
    request.body = body;
    request.content_type = !content_type.empty() ? content_type : net::constants::kDefaultContentType;
  }
};

// Body as JSON, default the type to "application/json", when `is_string_type<T>::value == false`.
template <typename REQUEST>
struct FillBody<REQUEST, false> {
  template <typename T>
  static std::enable_if_t<IS_CURRENT_STRUCT_OR_VARIANT(current::decay_t<T>)> Fill(
      REQUEST& request, T&& object, const std::string& content_type) {
    request.body = JSON(std::forward<T>(object));
    request.content_type = !content_type.empty() ? content_type : net::constants::kDefaultJSONContentType;
  }
};

struct POST : HTTPRequestBase<POST> {
  std::string body;
  std::string content_type;

  template <typename T>
  POST(const std::string& url, T&& body, const std::string& content_type = "")
      : HTTPRequestBase(url) {
    FillBody<POST, current::strings::is_string_type<T>::value>::Fill(*this, std::forward<T>(body), content_type);
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
    FillBody<PUT, current::strings::is_string_type<T>::value>::Fill(*this, std::forward<T>(body), content_type);
  }
};

struct PATCH : HTTPRequestBase<PATCH> {
  std::string body;
  std::string content_type;

  template <typename T>
  PATCH(const std::string& url, T&& body, const std::string& content_type = "")
      : HTTPRequestBase(url) {
    FillBody<PATCH, current::strings::is_string_type<T>::value>::Fill(*this, std::forward<T>(body), content_type);
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
  current::net::HTTPResponseCodeValue code;
  // HTTP response headers.
  current::net::http::Headers headers;
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
  using response_type_t = HTTPResponseWithBuffer;
};

template <>
struct ResponseTypeFromRequestType<SaveResponseToFile> {
  using response_type_t = HTTPResponseWithResultingFileName;
};

// To allow adding new implementation to the framework defined in api.h, the corresponding implementations
// should be added as `class FooImpl { ... }; template<> class ImplWrapper<FooImpl> { ... }`.
template <class T>
class ImplWrapper {};

// The main implementation of what `HTTP` actually is.
// The real work is done by templated implementations.
template <class CLIENT_IMPL, class CHUNKED_CLIENT_IMPL, class SERVER_IMPL>
struct HTTPImpl {
  typedef CLIENT_IMPL client_impl_t;
  typedef CHUNKED_CLIENT_IMPL chunked_client_impl_t;
  typedef SERVER_IMPL server_impl_t;

  server_impl_t& operator()(uint16_t port) {
    static std::mutex mutex;
    static std::map<uint16_t, std::unique_ptr<server_impl_t>> servers;
    std::lock_guard<std::mutex> lock(mutex);
    std::unique_ptr<server_impl_t>& server = servers[port];
    if (!server) {
      server.reset(new server_impl_t(port));
    }
    return *server;
  }

  // TODO(dkorolev): Deprecate the below some time in the future. And perhaps add an `http_port_t`.
  server_impl_t& operator()(int port) {
    CURRENT_ASSERT(port > 0 && port < 65536);
    return operator()(static_cast<uint16_t>(port));
  }

  template <typename REQUEST_PARAMS, typename RESPONSE_PARAMS = KeepResponseInMemory>
  inline typename ResponseTypeFromRequestType<RESPONSE_PARAMS>::response_type_t operator()(
      const REQUEST_PARAMS& request_params, const RESPONSE_PARAMS& response_params = RESPONSE_PARAMS()) const {
    typename client_impl_t::http_helper_t::ConstructionParams impl_params;
    client_impl_t impl(impl_params);
    typedef ImplWrapper<client_impl_t> IMPL_HELPER;
    IMPL_HELPER::PrepareInput(request_params, impl);
    IMPL_HELPER::PrepareInput(response_params, impl);
    if (!impl.Go()) {
#ifndef ANDROID
      CURRENT_THROW(current::net::HTTPException());  // LCOV_EXCL_LINE
#else
      // TODO(dkorolev): Chat with Alex. We can overcome the exception here, but should we?
      return typename ResponseTypeFromRequestType<RESPONSE_PARAMS>::response_type_t();
#endif
    }
    typename ResponseTypeFromRequestType<RESPONSE_PARAMS>::response_type_t output;
    IMPL_HELPER::ParseOutput(request_params, response_params, impl, output);
    return output;
  }

  inline net::HTTPResponseCodeValue operator()(const ChunkedGET& request_params) const {
    typename chunked_client_impl_t::http_helper_t::ConstructionParams impl_params(
        request_params.header_callback, request_params.chunk_callback, request_params.done_callback);
    chunked_client_impl_t impl(impl_params);
    ImplWrapper<client_impl_t>::PrepareInput(request_params, impl);
    if (impl.Go()) {
      return impl.response_code_;
    } else {
      return HTTPResponseCode.InvalidCode;
    }
  }

  inline net::HTTPResponseCodeValue operator()(const ChunkedPOST& request_params) const {
    typename chunked_client_impl_t::http_helper_t::ConstructionParams impl_params(
        request_params.header_callback, request_params.chunk_callback, request_params.done_callback);
    chunked_client_impl_t impl(impl_params);
    ImplWrapper<client_impl_t>::PrepareInput(request_params, impl);
    if (impl.Go()) {
      return impl.response_code_;
    } else {
      return HTTPResponseCode.InvalidCode;
    }
  }
};

}  // namespace http
}  // namespace current

using current::http::GET;
using current::http::HEAD;
using current::http::POST;
using current::http::POSTFromFile;
using current::http::PUT;
using current::http::PATCH;
using current::http::DELETE;

using current::http::ChunkedGET;
using current::http::ChunkedPOST;

#endif  // BLOCKS_HTTP_TYPES_H
