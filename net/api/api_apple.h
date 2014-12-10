// TODO(dkorolev): Check in the HTTP server here, the one as simple to use as this HTTP client.
// TODO(dkorolev): Merge exception type(s) into bricks/net.

#ifndef BRICKS_NET_API_APPLE_H
#define BRICKS_NET_API_APPLE_H

#if 0  //!defined(__APPLE__)
#error "`net/api_apple.h` should only be included in Apple builds."
#endif

#include "api.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <functional>

#include "impl_apple.h"

#include "../http.h"

using aloha::HTTPClientApple;

using bricks::net::Socket;
using bricks::net::HTTPConnection;
using bricks::net::HTTPHeadersType;
using bricks::net::HTTPResponseCode;

namespace bricks {
namespace net {
namespace api {

template <>
struct HTTPClientTemplatedImpl<HTTPClientApple> {
  // Populating the fields of HTTPClientApple given request parameters.
  inline static void PrepareInput(const HTTPRequestGET& request, HTTPClientApple& client) {
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
  }
  inline static void PrepareInput(const HTTPRequestPOST& request, HTTPClientApple& client) {
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
    client.set_post_body(request.body, request.content_type);
  }
  inline static void PrepareInput(const HTTPRequestPOSTFromFile& request, HTTPClientApple& client) {
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
    client.set_post_file(request.file_name, request.content_type);
  }

  // Populating the fields of HTTPClientApple given response configuration parameters.
  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientApple&) {
  }
  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientApple& client) {
    client.set_received_file(save_to_file_request.file_name);
  }

  // Parsing the response from within HTTPClientApple.
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientApple& response,
                                 HTTPResponse& output) {
    if (request_params.url != response.url_requested()) {
      throw HTTPClientException();
    }
    output.url = request_params.url;
    output.code = response.error_code();
    output.url_after_redirects = response.url_received();
    output.was_redirected = response.was_redirected();
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientApple& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    output.body = response.server_response();
  }
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientApple& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_APPLE_H
