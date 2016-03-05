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

#if defined(CURRENT_APPLE)

#include "../types.h"

namespace current {
namespace http {

struct HTTPClientApple {
  // Request data.
  std::string request_method = "";
  std::string request_url = "";
  current::net::HTTPHeadersType request_headers;
  std::string request_body = "";
  std::string request_body_content_type = "";
  std::string post_file = "";
  std::string request_user_agent = "";
  bool request_succeeded = false;
  current::WaitableAtomic<bool> async_request_completed;

  // Response data.
  int response_code = -1;
  std::string response_url = "";
  current::net::HTTPRequestData::HeadersType response_headers;
  std::string received_file = "";
  std::string response_body = "";

  inline bool Go();
};

template <>
struct ImplWrapper<HTTPClientApple> {
  // Populating the fields of HTTPClientApple given request parameters.
  inline static void PrepareInput(const GET& request, HTTPClientApple& client) {
    client.request_method = "GET";
    client.request_url = request.url;
    client.request_user_agent = request.custom_user_agent;
    client.request_headers = request.custom_headers;
  }

  inline static void PrepareInput(const POST& request, HTTPClientApple& client) {
    client.request_method = "POST";
    client.request_url = request.url;
    client.request_user_agent = request.custom_user_agent;
    client.request_headers = request.custom_headers;
    client.request_body = request.body;
    client.request_body_content_type = request.content_type;
  }

  inline static void PrepareInput(const POSTFromFile& request, HTTPClientApple& client) {
    client.request_method = "POST";
    client.request_url = request.url;
    client.request_user_agent = request.custom_user_agent;
    client.request_headers = request.custom_headers;
    client.post_file = request.file_name;
    client.request_body_content_type = request.content_type;
  }

  inline static void PrepareInput(const PUT& request, HTTPClientApple& client) {
    client.request_method = "PUT";
    client.request_url = request.url;
    client.request_user_agent = request.custom_user_agent;
    client.request_headers = request.custom_headers;
    client.request_body = request.body;
    client.request_body_content_type = request.content_type;
  }

  inline static void PrepareInput(const DELETE& request, HTTPClientApple& client) {
    client.request_method = "DELETE";
    client.request_url = request.url;
    client.request_user_agent = request.custom_user_agent;
    client.request_headers = request.custom_headers;
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
    output.url = response.response_url;
    output.code = HTTPResponseCode(response.response_code);
    output.headers = response.response_headers;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientApple& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    output.body = response.response_body;
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

}  // namespace http
}  // namespace current

#include "apple_client.mm"

#endif  // defined(CURRENT_APPLE)

#endif  // BLOCKS_HTTP_IMPL_APPLE_CLIENT_H
