// TODO(dkorolev): Check in the HTTP server here, the one as simple to use as this HTTP client.
// TODO(dkorolev): Merge exception type(s) into bricks/net.

#ifndef BRICKS_NET_API_APPLE_H
#define BRICKS_NET_API_APPLE_H

#if !defined(__APPLE__)
#error "`net/api_apple.h` should only be included in Apple builds."
#endif

#include <fstream>
#include <iostream>
#include <string>
#include <functional>

#include "http_client.h"

#include "../http.h"

using std::string;

using aloha::HTTPClientPlatformWrapper;

using bricks::net::Socket;
using bricks::net::HTTPConnection;
using bricks::net::HTTPHeadersType;
using bricks::net::HTTPResponseCode;

struct HTTPClientException : std::exception {};

struct HTTPRequestGET {
  string url;
  string custom_user_agent;
  explicit HTTPRequestGET(const string& url) : url(url) {
  }
  HTTPRequestGET& SetUserAgent(const string& ua) {
    custom_user_agent = ua;
    return *this;
  }
  void PopulateClient(HTTPClientPlatformWrapper& client) const {
    client.set_url_requested(url);
    if (!custom_user_agent.empty()) {
      client.set_user_agent(custom_user_agent);
    }
  }
};

struct HTTPRequestPOST {
  string url;
  string body;
  string content_type;
  explicit HTTPRequestPOST(const string& url, const string& body, const string& content_type)
      : url(url), body(body), content_type(content_type) {
  }
  void PopulateClient(HTTPClientPlatformWrapper& client) const {
    client.set_url_requested(url);
    client.set_post_body(body, content_type);
  }
};

struct HTTPRequestPOSTFromFile {
  string url;
  string file_name;
  string content_type;
  explicit HTTPRequestPOSTFromFile(const string& url, const string& file_name, const string& content_type)
      : url(url), file_name(file_name), content_type(content_type) {
  }
  void PopulateClient(HTTPClientPlatformWrapper& client) const {
    client.set_url_requested(url);
    client.set_post_file(file_name, content_type);
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

struct HTTPResponse {
  // Request.
  string url;

  // Response code.
  // Response body can be either `string body` or `string body_file_name`,
  // defined in `struct DownoadData` and `struct DownloadFile` respectively.
  int code;

  // Redirect metadata.
  // TODO(dkorolev): Add a test when the redirects end up pointing to the same original URL,
  //                 so that (url_after_redirects == url && was_redirected == true).
  string url_after_redirects;
  bool was_redirected;

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  void Populate(const T_REQUEST_PARAMS& request_params,
                const T_RESPONSE_PARAMS& response_params,
                const HTTPClientPlatformWrapper& response) {
    assert(request_params.url == response.url_requested());
    url = request_params.url;
    code = response.error_code();
    url_after_redirects = response.url_received();
    was_redirected = response.was_redirected();
  }
};

struct HTTPResponseWithBuffer : HTTPResponse {
  // Returned HTTP body, saved as an in-memory buffer, stored in std::string.
  string body;

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  void Populate(const T_REQUEST_PARAMS& request_params,
                const T_RESPONSE_PARAMS& response_params,
                const HTTPClientPlatformWrapper& response) {
    HTTPResponse::Populate(request_params, response_params, response);
    body = response.server_response();
  }
};

struct HTTPResponseWithResultingFileName : HTTPResponse {
  // The file name into which the returned HTTP body has been saved.
  string body_file_name;

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  void Populate(const T_REQUEST_PARAMS& request_params,
                const T_RESPONSE_PARAMS& response_params,
                const HTTPClientPlatformWrapper& response) {
    HTTPResponse::Populate(request_params, response_params, response);
    body_file_name = response_params.file_name;
  }
};

struct KeepResponseInMemory {
  void PopulateClient(HTTPClientPlatformWrapper& client) const {
  }
};

struct SaveResponseToFile {
  string file_name;
  explicit SaveResponseToFile(const string& file_name) : file_name(file_name) {
  }
  void PopulateClient(HTTPClientPlatformWrapper& client) const {
    client.set_received_file(file_name);
  }
};

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

template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS = KeepResponseInMemory>
inline typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE HTTP(
    const T_REQUEST_PARAMS& request_params,
    const T_RESPONSE_PARAMS& response_params = T_RESPONSE_PARAMS()) {
  HTTPClientPlatformWrapper client;
  request_params.PopulateClient(client);
  response_params.PopulateClient(client);
  if (!client.RunHTTPRequest()) {
    throw HTTPClientException();
  }
  typename ResponseTypeFromRequestType<T_RESPONSE_PARAMS>::T_RESPONSE_TYPE result;
  result.Populate(request_params, response_params, client);
  return result;
}

#endif  // BRICKS_NET_API_APPLE_H
