/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmai.com>

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

// TODO(dkorolev): Here and in other implementations, test sending empty BODY in POST-s.

#ifndef BRICKS_NET_API_APPLE_H
#define BRICKS_NET_API_APPLE_H

#include "../../../port.h"

#if defined(BRICKS_APPLE)

#include "../types.h"

namespace bricks {
namespace net {
namespace api {

struct HTTPClientApple {
  std::string url_requested = "";
  std::string url_received = "";
  int error_code = -1;
  std::string post_file = "";
  std::string received_file = "";
  std::string server_response = "";
  std::string content_type = "";
  std::string user_agent = "";
  std::string post_body = "";
  bool Go();
};

template <>
struct ImplWrapper<HTTPClientApple> {
  // Populating the fields of HTTPClientApple given request parameters.
  inline static void PrepareInput(const HTTPRequestGET& request, HTTPClientApple& client) {
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
  }
  inline static void PrepareInput(const HTTPRequestPOST& request, HTTPClientApple& client) {
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.post_body = request.body;
    client.content_type = request.content_type;
  }
  inline static void PrepareInput(const HTTPRequestPOSTFromFile& request, HTTPClientApple& client) {
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.post_file = request.file_name;
    client.content_type = request.content_type;
  }

  // Populating the fields of HTTPClientApple given response configuration parameters.
  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientApple&) {
  }
  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientApple& client) {
    client.received_file = save_to_file_request.file_name;
  }

  // Parsing the response from within HTTPClientApple.
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS&,
                                 const T_RESPONSE_PARAMS&,
                                 const HTTPClientApple& response,
                                 HTTPResponse& output) {
    output.url = response.url_requested;
    output.code = response.error_code;
    output.url_after_redirects = response.url_received;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientApple& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    output.body = response.server_response;
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

#include "apple.mm"

#endif  // defined(BRICKS_APPLE)

#endif  // BRICKS_NET_API_APPLE_H
