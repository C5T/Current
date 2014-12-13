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
// ## const std::string& s = HTTP(GET(url)).body;
// ## const auto r = HTTP(GET(url)); DoWork(r.code, r.body);
// ## const auto r = HTTP(GET(url), SaveResponseToFile(file_name)); DoWork(r.code, r.body_file_name);
// ## const auto r = HTTP(POST(url, "data", "text/plain")); DoWork(r.code);
// ## const auto r = HTTP(POSTFromFile(url, file_name, "text/plain")); DoWork(r.code);
//                   TODO(dkorolev): Hey Alex, do we support returned body from POST requests? :-)
//
// # SERVER: TODO(dkorolev).
//
// Purpose of this file: To ensure that each header can compile on its own, thus passing the `make check`
// target.
// This requires each header, including api_{apple,posix,etc}.h, to contain all their dependencies.
// If the code below would be in api.h, api_{apple,posix,etc}.h would contain an undesired cyclical dependency.

#ifndef BRICKS_NET_API_TYPES_H
#define BRICKS_NET_API_TYPES_H

#include <string>

namespace bricks {
namespace net {
namespace api {

// HTTP exceptions.
// TODO(dkorolev): Structure the exceptions. Make them all eventually inherit from bricks::Exception.

struct HTTPClientException : std::exception {};

// Structures to define HTTP requests.
// Support GET and POST.
// The syntax for creating an instance of a GET request is GET is `GET(url)`.
// The syntax for creating an instance of a POST request is POST is `POST(url, data, content_type)`'.
// Alternatively, `POSTFromFile(url, file_name, content_type)` is supported.
// Both GET and two forms of POST allow `.SetUserAgent(custom_user_agent)`.

struct HTTPRequestGET {
  std::string url;
  std::string custom_user_agent;

  explicit HTTPRequestGET(const std::string& url) : url(url) {
  }

  HTTPRequestGET& SetUserAgent(const std::string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

struct HTTPRequestPOST {
  std::string url;
  std::string custom_user_agent;
  std::string body;
  std::string content_type;

  explicit HTTPRequestPOST(const std::string& url, const std::string& body, const std::string& content_type)
      : url(url), body(body), content_type(content_type) {
  }

  HTTPRequestPOST& SetUserAgent(const std::string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

struct HTTPRequestPOSTFromFile {
  std::string url;
  std::string custom_user_agent;
  std::string file_name;
  std::string content_type;

  explicit HTTPRequestPOSTFromFile(const std::string& url,
                                   const std::string& file_name,
                                   const std::string& content_type)
      : url(url), file_name(file_name), content_type(content_type) {
  }

  HTTPRequestPOSTFromFile& SetUserAgent(const std::string& ua) {
    custom_user_agent = ua;
    return *this;
  }
};

HTTPRequestGET GET(const std::string& url) {
  return HTTPRequestGET(url);
}

HTTPRequestPOST POST(const std::string& url, const std::string& body, const std::string& content_type) {
  return HTTPRequestPOST(url, body, content_type);
}

HTTPRequestPOSTFromFile POSTFromFile(const std::string& url,
                                     const std::string& file_name,
                                     const std::string& content_type) {
  return HTTPRequestPOSTFromFile(url, file_name, content_type);
}

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
  explicit SaveResponseToFile(const std::string& file_name) : file_name(file_name) {
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

// To allow adding new implementation to the framework defined in api.h, the corresponding implementations
// should be added as tempate specializations of `class TemplatePopulateClientImpl<Impl>`.
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

#endif  // BRICKS_NET_API_TYPES_H
