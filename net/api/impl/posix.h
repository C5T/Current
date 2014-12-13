// TODO(dkorolev)+TODO(deathbaba): Agree that empty POST body should throw and exception and throw it.

#ifndef BRICKS_NET_API_POSIX_H
#define BRICKS_NET_API_POSIX_H

#include "../types.h"
#include "../url.h"

#include <memory>
#include <string>
#include <set>

#include "../../http.h"
#include "../../../file/file.h"

namespace bricks {
namespace net {
namespace api {

class HTTPClientPOSIX final {
 private:
  struct HTTPRedirectHelper : HTTPDefaultHelper {
    std::string location = "";
    inline void OnHeader(const char* key, const char* value) {
      if (std::string("Location") == key) {
        location = value;
      }
    }
  };
  typedef TemplatedHTTPReceivedMessage<HTTPRedirectHelper> HTTPRedirectableReceivedMessage;

 public:
  // The actual implementation.
  bool Go() {
    // TODO(dkorolev): Always use the URL returned by the server here.
    response_url_after_redirects_ = request_url_;
    URLParser parsed_url(request_url_);
    std::set<std::string> all_urls;
    bool redirected;
    do {
      redirected = false;
      if (all_urls.count(parsed_url.ComposeURL())) {
        throw new HTTPRedirectLoopException();
      }
      all_urls.insert(parsed_url.ComposeURL());
      Connection connection(Connection(ClientSocket(parsed_url.host, parsed_url.port)));
      connection.BlockingWrite(request_method_ + ' ' + parsed_url.path + " HTTP/1.1\r\n");
      connection.BlockingWrite("Host: " + parsed_url.host + "\r\n");
      if (!request_user_agent_.empty()) {
        connection.BlockingWrite("User-Agent: " + request_user_agent_ + "\r\n");
      }
      if (!request_body_content_type_.empty()) {
        connection.BlockingWrite("Content-Type: " + request_body_content_type_ + "\r\n");
      }
      connection.BlockingWrite("Content-Length: " + std::to_string(request_body_contents_.length()) + "\r\n");
      connection.BlockingWrite("\r\n");
      connection.BlockingWrite(request_body_contents_);
      // Attention! Achtung! Увага! Внимание!
      // Calling SendEOF() (which is ::shutdown(socket, SHUT_WR);) results in slowly sent data
      // not being received. Tested on local and remote data with "chunked" transfer encoding.
      // Don't uncomment the next line!
      // connection.SendEOF();
      message_.reset(new HTTPRedirectableReceivedMessage(connection));
      response_code_ =
          atoi(message_->URL().c_str());  // TODO(dkorolev): Rename URL() to a more meaningful thing.
      if (response_code_ >= 300 && response_code_ <= 399 && !message_->location.empty()) {
        // TODO(dkorolev): Open at least one manual page about redirects before merging this code.
        redirected = true;
        parsed_url = URLParser(message_->location, parsed_url);
        response_url_after_redirects_ = parsed_url.ComposeURL();
      }
    } while (redirected);
    return true;
  }

  const HTTPRedirectableReceivedMessage& GetMessage() const {
    return *message_.get();
  }

 public:
  // Request parameters.
  std::string request_method_ = "";
  std::string request_url_ = "";
  std::string request_body_content_type_ = "";
  std::string request_body_contents_ = "";
  std::string request_user_agent_ = "";

  // Output parameters.
  int response_code_ = -1;
  std::string response_url_after_redirects_ = "";

 private:
  std::unique_ptr<HTTPRedirectableReceivedMessage> message_;
};

template <>
struct ImplWrapper<HTTPClientPOSIX> {
  inline static void PrepareInput(const HTTPRequestGET& request, HTTPClientPOSIX& client) {
    client.request_method_ = "GET";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;
    }
  }

  inline static void PrepareInput(const HTTPRequestPOST& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;
    }
    client.request_body_contents_ = request.body;
    client.request_body_content_type_ = request.content_type;
  }

  inline static void PrepareInput(const HTTPRequestPOSTFromFile& request, HTTPClientPOSIX& client) {
    client.request_method_ = "POST";
    client.request_url_ = request.url;
    if (!request.custom_user_agent.empty()) {
      client.request_user_agent_ = request.custom_user_agent;
    }
    try {
      client.request_body_contents_ = ReadFileAsString(request.file_name);
      client.request_body_content_type_ = request.content_type;
    } catch (FileException&) {
      // TODO(dkorolev): Unfix this "fix" once we use proper exceptions in other clients (Apple, Android).
      throw HTTPClientException();
    }
    client.request_body_content_type_ = request.content_type;
  }

  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientPOSIX&) {
  }

  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientPOSIX&) {
    assert(!save_to_file_request.file_name.empty());
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& /*response_params*/,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponse& output) {
    if (request_params.url != response.request_url_) {
      throw HTTPClientException();
    }
    output.url = request_params.url;
    output.code = response.response_code_;
    output.url_after_redirects = response.response_url_after_redirects_;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    const auto& message = response.GetMessage();
    output.body = message.HasBody() ? message.Body() : "";
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    // TODO(dkorolev): This is doubly inefficient. Gotta write the buffer or write chunks.
    const auto& message = response.GetMessage();
    WriteStringToFile(response_params.file_name, message.HasBody() ? message.Body() : "");
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_POSIX_H
