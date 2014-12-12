// TODO(dkorolev): Clean up this file.

#ifndef BRICKS_NET_API_POSIX_H
#define BRICKS_NET_API_POSIX_H

#include "api.h"

namespace bricks {
namespace net {
namespace api {

struct HTTPClientPOSIX {
  bool Go() {
    return true;
  }
};

template <>
struct HTTPClientTemplatedImpl<HTTPClientPOSIX> {
  // Populating the fields of HTTPClientPOSIX given request parameters.
  inline static void PrepareInput(const HTTPRequestGET& request, HTTPClientPOSIX& client) {
    /*
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
    */
  }
  inline static void PrepareInput(const HTTPRequestPOST& request, HTTPClientPOSIX& client) {
    /*
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
    client.set_post_body(request.body, request.content_type);
    */
  }
  inline static void PrepareInput(const HTTPRequestPOSTFromFile& request, HTTPClientPOSIX& client) {
    /*
    client.set_url_requested(request.url);
    if (!request.custom_user_agent.empty()) {
      client.set_user_agent(request.custom_user_agent);
    }
    client.set_post_file(request.file_name, request.content_type);
    */
  }

  // Populating the fields of HTTPClientPOSIX given response configuration parameters.
  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientPOSIX&) {
  }
  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientPOSIX& client) {
    // client.set_received_file(save_to_file_request.file_name);
  }

  // Parsing the response from within HTTPClientPOSIX.
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponse& output) {
    /*
    if (request_params.url != response.url_requested()) {
      throw HTTPClientException();
    }
    output.url = request_params.url;
    output.code = response.error_code();
    output.url_after_redirects = response.url_received();
    output.was_redirected = response.was_redirected();
    */
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    // output.body = response.server_response();
  }
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_POSIX_H
