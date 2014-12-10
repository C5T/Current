// Bricks HTTP API provides a generic interface for basic HTTP operations,
// along with their cross-platform implementations.
//
// HTTP server has a TODO(dkorolev) implementation.
// HTTP client has POSIX, MacOS/iOS and TODO(deathbaba) implementations.
//
// The user does not have to select the implementation, a suitable one will be selected at compile time.
//
// For unit testing, a type list with available implementations is generated per architecture as well.
// For example, MacOS can run both Mac-native and POSIX implementations of HTTP client.
//
// Synopsis:
//
// # CLIENT: Returns template-specialized type depending on its parameters. Best to save into `const auto`.
// ## const string& s = HTTP(GET(url)).body;
// ## const auto r = HTTP(GET(url)); Process(r.code, r.body);
// ## const auto r = HTTP(GET(url), SaveResponseToFile(file_name)); Process(r.code, r.body_file_name);
// ## const auto r = HTTP(POST(url, "data", "text/plain")); Process(r.code);
// ## const auto r = HTTP(POSTFromFile(url, file_name, "text/plain")); Process(r.code);
//                   TODO(dkorolev): Hey Alex, do we support returned body from POST requests? :-)
//
// # SERVER: TODO(dkorolev).

#ifndef BRICKS_NET_API_H
#define BRICKS_NET_API_H

#include <string>

using std::string;

namespace bricks {
namespace net {
namespace api {

// HTTP exceptions.
// TODO(dkorolev): Structure the exceptions. Make them all inherit from some BricksException.

struct HTTPClientException : std::exception {};

// Structures to define HTTP requests.
// Support GET and POST.
// The syntax for creating an instance of a GET request is GET is `GET(url)`.
// The syntax for creating an instance of a POST request is POST is `POST(url, data, content_type)`'.
// Alternatively, `POSTFromFile(url, file_name, content_type)` is supported.
// Both GET and two forms of POST allow `.SetUserAgent(custom_user_agent)`.

struct HTTPRequestGET {
  string url;
  string custom_user_agent;

  explicit HTTPRequestGET(const string& url) : url(url) {
  }

  HTTPRequestGET& SetUserAgent(const string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

struct HTTPRequestPOST {
  string url;
  string custom_user_agent;
  string body;
  string content_type;

  explicit HTTPRequestPOST(const string& url, const string& body, const string& content_type)
      : url(url), body(body), content_type(content_type) {
  }

  HTTPRequestPOST& SetUserAgent(const string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

struct HTTPRequestPOSTFromFile {
  string url;
  string custom_user_agent;
  string file_name;
  string content_type;

  explicit HTTPRequestPOSTFromFile(const string& url, const string& file_name, const string& content_type)
      : url(url), file_name(file_name), content_type(content_type) {
  }

  HTTPRequestPOSTFromFile& SetUserAgent(const string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

HTTPRequestGET GET(const string& url) {
  return HTTPRequestGET(url);
}

HTTPRequestPOST POST(const string& url, const string& body, const string& content_type) {
  return HTTPRequestPOST(url, body, content_type);
}

HTTPRequestPOSTFromFile POSTFromFile(const string& url, const string& file_name, const string& content_type) {
  return HTTPRequestPOSTFromFile(url, file_name, content_type);
}

// Structures to define HTTP response.
// The actual response type is templated and depends on the types of input paramteres.
// All responses inherit from `struct HTTPResponse`, that lists common fields.

struct HTTPResponse {
  string url;                  // Request.
  int code;                    // Response code. TODO(dkorolev): HTTPResponseCode from ../http/codes.h?
  string url_after_redirects;  // The final URL after all the redirects.
  bool was_redirected;         // Whether there have been any redirects.
};

struct HTTPResponseWithBuffer : HTTPResponse {
  string body;  // Returned HTTP body, saved as an in-memory buffer, stored in std::string.
};

struct HTTPResponseWithResultingFileName : HTTPResponse {
  string body_file_name;  // The file name into which the returned HTTP body has been saved.
};

// Response storage policy.
// The default one is `KeepResponseInMemory()`, which can be omitted.
// The alternative one is `SaveResponseToFile(file_name)`.

struct KeepResponseInMemory {};

struct SaveResponseToFile {
  string file_name;
  explicit SaveResponseToFile(const string& file_name) : file_name(file_name) {
  }
};

// Template metaprogramming for selecting the right types at compile time,
// taking into account the request type and response type, as well well allowing extensibility
// with respect to adding new interfaces.

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

// Boilerplate for various implementations of HTTP clients.
//
// The actual implementation consists of a class Impl that has a member function `Impl::RunHTTPRequest()`.
// Also, an instance of class Impl should be populatable by both an instance of an HTTP request parameters
// and an instance of HTTP response parameters. On top of the above, HTTP response should be parsable
// from an instance of this class.
//
// To have the above extensible, the corresponding implementations should be added
// as tempate specializations to `class TemplatePopulateClientImpl<Impl>`.
template <class T>
class HTTPClientTemplatedImpl {};

// The main implementation of what `HTTP` actually is.
// The real work is done by templated implementations.
template <typename T_IMPLEMENTATION_TO_USE>
struct HTTPClientImpl {
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS = KeepResponseInMemory>
  inline typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE operator()(
      const T_REQUEST_PARAMS& request_params,
      const T_RESPONSE_PARAMS& response_params = T_RESPONSE_PARAMS()) {
    T_IMPLEMENTATION_TO_USE impl;
    typedef HTTPClientTemplatedImpl<T_IMPLEMENTATION_TO_USE> IMPL_HELPER;
    IMPL_HELPER::PrepareInput(request_params, impl);
    IMPL_HELPER::PrepareInput(response_params, impl);
    if (!impl.Go()) {
      throw HTTPClientException();
    }
    typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE output;
    IMPL_HELPER::ParseOutput(request_params, response_params, impl, output);
    return output;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

// Actual implementations, guarded by preprpocessor directives.

// TODO(dkorolev): Revisit these guards once we go beyond Ubuntu & Mac.
#if defined(__APPLE__)
#include "api_apple.h"
bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientApple> HTTP;
#elif defined(__linux)
#include "api_posix.h"
bricks::net::api::HTTPClientImpl<bricks::net::api::HTTPClientPOSIX> HTTP;
#else
#error "No implementation for `net/api/api.h` is available for your system."
#endif

#endif  // BRICKS_NET_API_H
