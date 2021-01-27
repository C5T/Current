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

// TODO(dkorolev): Handle empty POST body. Add a test for it.
// TODO(dkorolev): Support receiving body via POST requests. Add a test for it.

#ifndef BLOCKS_HTTP_IMPL_POSIX_CLIENT_H
#define BLOCKS_HTTP_IMPL_POSIX_CLIENT_H

#include <memory>
#include <string>
#include <set>

#include "../types.h"
#include "../chunked_response_parser.h"

#include "../../url/url.h"

#include "../../../bricks/net/http/http.h"
#include "../../../bricks/file/file.h"

namespace current {
namespace http {

namespace impl {
struct HTTPRedirectHelper : current::net::HTTPDefaultHelper {
  struct ConstructionParams {};
  HTTPRedirectHelper() = delete;
  HTTPRedirectHelper(const ConstructionParams&) {}

  std::string location = "";

  inline void OnHeader(const char* key, const char* value, current::net::HTTPResponseKind& response_kind) {
    if (std::string("Location") == key) {
      location = value;
    }
    current::net::HTTPDefaultHelper::OnHeader(key, value, response_kind);
  }
};
}  // namespace current::http::impl

template <class HTTP_HELPER>
class GenericHTTPClientPOSIX final {
 private:
  using CustomHTTPRequestData = current::net::GenericHTTPRequestData<HTTP_HELPER>;

 public:
  using http_helper_t = HTTP_HELPER;
  GenericHTTPClientPOSIX(const typename HTTP_HELPER::ConstructionParams& params)
      : request_data_construction_params_(params) {}

  // The actual implementation.
  bool Go() {
    // TODO(dkorolev): Always use the URL returned by the server here.
    response_url_after_redirects_ = request_url_;
    URL parsed_url(request_url_);
    std::set<std::string> all_urls;
    bool redirected;
    do {
      redirected = false;
      const std::string composed_url = parsed_url.ComposeURL();
      if (all_urls.count(composed_url)) {
        const std::string loop = (current::strings::Join(all_urls, ' ') + " " + composed_url);
        CURRENT_THROW(current::net::HTTPRedirectLoopException(loop));
      }
      all_urls.insert(composed_url);
      // The `URL` class' concern is to parse the URL string as is: if the port is missing, it becomes zero.
      // `URL::DefaultPortForScheme` will return zero anyway if the schema is unknown.
      // So, to make sure we don't try to connect to port zero, the defaulting logic has to be here in the HTTP client.
      int port = parsed_url.port;
      if (port == 0) {
        port = URL::DefaultPortForScheme(parsed_url.scheme);
        if (port == 0) {
          port = 80;
        }
      }
      current::net::Connection connection(
          current::net::Connection(current::net::ClientSocket(parsed_url.host, port)));
      connection.BlockingWrite(
          request_method_ + ' ' + parsed_url.path + parsed_url.ComposeParameters() + " HTTP/1.1\r\n", true);
      connection.BlockingWrite("Host: " + parsed_url.host + "\r\n", true);
      if (!request_user_agent_.empty()) {
        connection.BlockingWrite("User-Agent: " + request_user_agent_ + "\r\n", true);
      }
      for (const auto& h : request_headers_) {
        connection.BlockingWrite(h.header + ": " + h.value + "\r\n", true);
      }
      if (!request_headers_.cookies.empty()) {
        connection.BlockingWrite("Cookie: " + request_headers_.CookiesAsString() + "\r\n", true);
      }
      if (!request_body_content_type_.empty()) {
        connection.BlockingWrite("Content-Type: " + request_body_content_type_ + "\r\n", true);
      }
      if (!request_body_contents_.empty() || current::net::NeedContentLengthHeader(request_method_)) {
        // NOTE(dkorolev): The `try/catch/throw` combo here is a hack for the unit test for HTTP 413 to pass.
        // It swallows the `SocketWriteException` exception for huge payloads, as Current's HTTP server logic
        // does intentionally close the HTTP connection prematurely if `Content-Length` exceeds a reasonable limit.
        try {
#ifndef CURRENT_WINDOWS
          connection.BlockingWrite("Content-Length: " + std::to_string(request_body_contents_.length()) + "\r\n", true);
          connection.BlockingWrite("\r\n", true);
          connection.BlockingWrite(request_body_contents_, false);
#else
          // TODO(grixa): this fix for the PayloadTooLarge test on Windows is temporary, need to revisit it.
          connection.BlockingWrite("Content-Length: " + std::to_string(request_body_contents_.length()) + "\r\n\r\n" +
                                       request_body_contents_,
                                   false);
#endif
        } catch (const net::SocketWriteException&) {
          if (request_body_contents_.length() <= net::constants::kMaxHTTPPayloadSizeInBytes) {
            throw;
          }
        }
      } else {
        connection.BlockingWrite("\r\n", false);
      }
      http_request_ = std::make_unique<CustomHTTPRequestData>(connection, request_data_construction_params_);
      // TODO(dkorolev): Rename `Path()`, it's only called so now because of HTTP request/response format.
      // Elaboration:
      // HTTP request  message is: `GET /path HTTP/1.1`, "/path" is the second component of it.
      // HTTP response message is: `HTTP/1.1 200 OK`, "200" is the second component of it.
      // Thus, since the same code is used for request and response parsing as of now,
      // the numerical response code "200" can be accessed with the same method as the "/path".
      const int response_code_as_int = atoi(http_request_->RawPath().c_str());
      response_code_ = HTTPResponseCode(response_code_as_int);
      // Follow the redirects automatically.
      // Note: This is by no means a complete redirect implementation.
      if (response_code_as_int >= 300 && response_code_as_int <= 399 && !http_request_->location.empty()) {
        if (!allow_redirects_) {
          CURRENT_THROW(current::net::HTTPRedirectNotAllowedException());
        }
        redirected = true;
        parsed_url = URL::MakeRedirectedURL(parsed_url, http_request_->location);
        response_url_after_redirects_ = parsed_url.ComposeURL();
      }
    } while (redirected);
    return true;
  }

  const CustomHTTPRequestData& HTTPRequest() const { return *http_request_.get(); }

 public:
  // Request parameters.
  std::string request_method_ = "";
  std::string request_url_ = "";
  std::string request_body_content_type_ = "";
  std::string request_body_contents_ = "";
  std::string request_user_agent_ = "";
  current::net::http::Headers request_headers_;
  const typename HTTP_HELPER::ConstructionParams request_data_construction_params_;
  bool allow_redirects_ = false;

  // Output parameters.
  current::net::HTTPResponseCodeValue response_code_ = HTTPResponseCode.InvalidCode;
  std::string response_url_after_redirects_ = "";

 private:
  std::unique_ptr<CustomHTTPRequestData> http_request_;
};

using HTTPClientPOSIX = GenericHTTPClientPOSIX<impl::HTTPRedirectHelper>;

template <>
struct ImplWrapper<HTTPClientPOSIX> {
  inline static void PrepareInput(const GET& request, HTTPClientPOSIX& client) {
    client.request_method_ = "GET";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;
    client.request_headers_ = request.custom_headers;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const HEAD& request, HTTPClientPOSIX& client) {
    client.request_method_ = "HEAD";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;
    client.request_headers_ = request.custom_headers;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const POST& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    client.request_headers_ = request.custom_headers;
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const POSTFromFile& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    client.request_headers_ = request.custom_headers;
    client.request_body_contents_ =
        current::FileSystem::ReadFileAsString(request.file_name);  // Can throw FileException.
    client.request_body_content_type_ = request.content_type;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const PUT& request, HTTPClientPOSIX& client) {
    client.request_method_ = "PUT";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    client.request_headers_ = request.custom_headers;
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const PATCH& request, HTTPClientPOSIX& client) {
    client.request_method_ = "PATCH";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    client.request_headers_ = request.custom_headers;
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const DELETE& request, HTTPClientPOSIX& client) {
    client.request_method_ = "DELETE";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;  // LCOV_EXCL_LINE  -- tested in GET above.
    client.request_headers_ = request.custom_headers;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const ChunkedGET& request,
                                  current::http::GenericHTTPClientPOSIX<ChunkByChunkHTTPResponseReceiver>& client) {
    client.request_method_ = "GET";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;
    client.request_headers_ = request.custom_headers;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const ChunkedPOST& request,
                                  current::http::GenericHTTPClientPOSIX<ChunkByChunkHTTPResponseReceiver>& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    client.request_user_agent_ = request.custom_user_agent;
    client.request_headers_ = request.custom_headers;
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
    client.allow_redirects_ = request.allow_redirects;
  }

  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientPOSIX&) {}

  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientPOSIX&) {
    CURRENT_ASSERT(!save_to_file_request.file_name.empty());
  }

  template <typename REQUEST_PARAMS, typename RESPONSE_PARAMS>
  inline static void ParseOutput(const REQUEST_PARAMS& request_params,
                                 const RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponse& output) {
    static_cast<void>(request_params);
    static_cast<void>(response_params);
    output.url = response.response_url_after_redirects_;
    output.code = response.response_code_;
    const auto& http_request = response.HTTPRequest();
    output.headers = http_request.headers();
  }

  template <typename REQUEST_PARAMS, typename RESPONSE_PARAMS>
  inline static void ParseOutput(const REQUEST_PARAMS& request_params,
                                 const RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    const auto& http_request = response.HTTPRequest();
    output.body = http_request.Body();
  }

  template <typename REQUEST_PARAMS, typename RESPONSE_PARAMS>
  inline static void ParseOutput(const REQUEST_PARAMS& request_params,
                                 const RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    // TODO(dkorolev): This is doubly inefficient. Should write the buffer or write in chunks instead.
    const auto& http_request = response.HTTPRequest();
    current::FileSystem::WriteStringToFile(http_request.Body(), response_params.file_name.c_str());
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace http
}  // namespace current

#endif  // BLOCKS_HTTP_IMPL_POSIX_CLIENT_H
