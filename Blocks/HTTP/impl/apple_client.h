/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus
          (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BLOCKS_HTTP_IMPL_APPLE_CLIENT_H
#define BLOCKS_HTTP_IMPL_APPLE_CLIENT_H

#include "../../../port.h"

#include "../../../Bricks/waitable_atomic/waitable_atomic.h"

#if defined(BRICKS_APPLE)

#include "../types.h"

namespace blocks {

struct HTTPClientApple {
  std::string url_requested = "";
  std::string url_received = "";
  int http_response_code = -1;
  std::string method = "";
  std::string post_body = "";
  std::string post_file = "";
  std::string received_file = "";
  std::string server_response = "";
  std::string content_type = "";
  std::string user_agent = "";
  bool request_succeeded = false;
  bricks::WaitableAtomic<bool> async_request_completed;

  inline bool Go();
};

template <>
struct ImplWrapper<HTTPClientApple> {
  // Populating the fields of HTTPClientApple given request parameters.
  inline static void PrepareInput(const GET& request, HTTPClientApple& client) {
    client.method = "GET";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
  }

  inline static void PrepareInput(const POST& request, HTTPClientApple& client) {
    client.method = "POST";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.post_body = request.body;
    client.content_type = request.content_type;
  }

  inline static void PrepareInput(const POSTFromFile& request, HTTPClientApple& client) {
    client.method = "POST";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.post_file = request.file_name;
    client.content_type = request.content_type;
  }

  inline static void PrepareInput(const PUT& request, HTTPClientApple& client) {
    client.method = "PUT";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.post_body = request.body;
    client.content_type = request.content_type;
  }

  inline static void PrepareInput(const DELETE& request, HTTPClientApple& client) {
    client.method = "DELETE";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
  }

  // Populating the fields of HTTPClientApple given response configuration parameters.
  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientApple&) {}
  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientApple& client) {
    client.received_file = save_to_file_request.file_name;
  }

  // Parsing the response from within HTTPClientApple.
  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS&,
                                 const T_RESPONSE_PARAMS&,
                                 const HTTPClientApple& response,
                                 HTTPResponse& output) {
    // TODO(dkorolev): Handle redirects in Apple implementation.
    output.url = response.url_received;
    output.code = HTTPResponseCode(response.http_response_code);
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

}  // namespace blocks

#include "apple_client.mm"

#endif  // defined(BRICKS_APPLE)

#endif  // BLOCKS_HTTP_IMPL_APPLE_CLIENT_H
