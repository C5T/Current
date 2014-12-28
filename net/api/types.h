// Please refer to the header comment in `api.h` for the details.
// This `types.h` file is used to remove cyclic dependency between impl/{posix,apple,etc}.h and api.h.

#ifndef BRICKS_NET_API_TYPES_H
#define BRICKS_NET_API_TYPES_H

#include <string>

#include "../../exception.h"

namespace bricks {
namespace net {
namespace api {

// HTTP exceptions.
// TODO(dkorolev): Structure HTTP exceptions.
struct HTTPClientException : Exception {};

// Structures to define HTTP requests.
// Support GET and POST.
// The syntax for creating an instance of a GET request is GET is `GET(url)`.
// The syntax for creating an instance of a POST request is POST is `POST(url, data, content_type)`'.
// Alternatively, `POSTFromFile(url, file_name, content_type)` is supported.
// Both GET and two forms of POST allow `.SetUserAgent(custom_user_agent)`.
struct GET {
  std::string url;
  std::string custom_user_agent;

  explicit GET(const std::string& url) : url(url) {}

  GET& SetUserAgent(const std::string& new_custom_user_agent) {
    custom_user_agent = new_custom_user_agent;
    return *this;
  }
};

struct POST {
  std::string url;
  std::string custom_user_agent;
  std::string body;
  std::string content_type;

  explicit POST(const std::string& url, const std::string& body, const std::string& content_type)
      : url(url), body(body), content_type(content_type) {}

  POST& SetUserAgent(const std::string& new_custom_user_agent) {
    custom_user_agent = new_custom_user_agent;
    return *this;
  }
};

struct POSTFromFile {
  std::string url;
  std::string custom_user_agent;
  std::string file_name;
  std::string content_type;

  explicit POSTFromFile(const std::string& url, const std::string& file_name, const std::string& content_type)
      : url(url), file_name(file_name), content_type(content_type) {}

  POSTFromFile& SetUserAgent(const std::string& new_custom_user_agent) {
    custom_user_agent = new_custom_user_agent;
    return *this;
  }
};

// Structures to define HTTP response.
// The actual response type is templated and depends on the types of input paramteres.
// All responses inherit from `struct HTTPResponse`, that lists common fields.
struct HTTPResponse {
  std::string url;                  // The original URL requested by the client.
  int code;                         // Response code. TODO(dkorolev): HTTPResponseCode from ../http/codes.h?
  std::string url_after_redirects;  // The final URL after all the redirects.
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
template <typename T_IMPLEMENTATION_TO_USE>
struct HTTPClientImpl {
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS = KeepResponseInMemory>
  inline typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE operator()(
      const T_REQUEST_PARAMS& request_params,
      const T_RESPONSE_PARAMS& response_params = T_RESPONSE_PARAMS()) const {
    T_IMPLEMENTATION_TO_USE impl;
    typedef ImplWrapper<T_IMPLEMENTATION_TO_USE> IMPL_HELPER;
    IMPL_HELPER::PrepareInput(request_params, impl);
    IMPL_HELPER::PrepareInput(response_params, impl);
    if (!impl.Go()) {
#ifndef ANDROID
      throw HTTPClientException();  // LCOV_EXCL_LINE
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

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_TYPES_H
