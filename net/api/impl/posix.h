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

using std::set;
using std::string;
using std::to_string;
using std::unique_ptr;

namespace bricks {
namespace net {
namespace api {

struct HTTPClientPOSIX final {
  // Request parameters.
  string method = "";
  string url_requested = "";
  enum { None = 0, BodyFromBuffer, BodyFromFile } request_body_type;
  string request_body_content_type = "";
  string request_body_contents = "";   // If request_body_type == BodyFromBuffer.
  string request_body_file_name = "";  // If request_body_type == BodyFromFile.
  enum { DestinationBuffer = 0, DestinationFile } destination_type;
  string destination_file_name;  // If destination_type == DestinationFile.
  unique_ptr<ScopedFileCleanup> file_cleanup_helper;
  string user_agent = "";

  // Output parameters.
  int code_ = -1;
  string url_after_redirects_ = "";

  struct HTTPRedirectHelper : HTTPDefaultHelper {
    string location = "";
    inline void OnHeader(const char* key, const char* value) {
      if (string("Location") == key) {
        location = value;
      }
    }
  };
  typedef TemplatedHTTPReceivedMessage<HTTPRedirectHelper> HTTPRedirectableReceivedMessage;
  std::unique_ptr<HTTPRedirectableReceivedMessage> message;

  // The actual implementation.
  bool Go() {
    url_after_redirects_ = url_requested;
    URLParser parsed_url(url_requested);
    set<string> all_urls;
    bool redirected;
    do {
      redirected = false;
      if (all_urls.count(parsed_url.ComposeURL())) {
        throw new HTTPRedirectLoopException();
      }
      all_urls.insert(parsed_url.ComposeURL());
      Connection connection(Connection(ClientSocket(parsed_url.host, parsed_url.port)));
      connection.BlockingWrite(method + ' ' + parsed_url.route + " HTTP/1.1\r\n");
      connection.BlockingWrite("Host: " + parsed_url.host + "\r\n");
      if (request_body_type == BodyFromBuffer) {
        if (!request_body_content_type.empty()) {
          connection.BlockingWrite("Content-Type: " + request_body_content_type + "\r\n");
        }
        connection.BlockingWrite("Content-Length: " + to_string(request_body_contents.length()) + "\r\n");
      }
      connection.BlockingWrite("\r\n");
      if (request_body_type == BodyFromBuffer) {
        connection.BlockingWrite(request_body_contents);
      }
      connection.SendEOF();
      message.reset(new HTTPRedirectableReceivedMessage(connection));
      code_ = atoi(message->URL().c_str());  // TODO(dkorolev): Rename URL() to a more meaningful thing.
      if (code_ >= 300 && code_ <= 399 && !message->location.empty()) {
        // TODO(dkorolev): Open at least one manual page about redirects before merging this code.
        redirected = true;
        parsed_url = URLParser(message->location, parsed_url);
        url_after_redirects_ = parsed_url.ComposeURL();
      }
    } while (redirected);
    return true;
  }
};

template <>
struct ImplWrapper<HTTPClientPOSIX> {
  inline static void PrepareInput(const HTTPRequestGET& request, HTTPClientPOSIX& client) {
    client.method = "GET";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
  }

  inline static void PrepareInput(const HTTPRequestPOST& request, HTTPClientPOSIX& client) {
    client.method = "POST";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.request_body_type = HTTPClientPOSIX::BodyFromBuffer;
    client.request_body_contents = request.body;
    client.request_body_content_type = request.content_type;
  }

  inline static void PrepareInput(const HTTPRequestPOSTFromFile& request, HTTPClientPOSIX& client) {
    client.method = "POST";
    client.url_requested = request.url;
    if (!request.custom_user_agent.empty()) {
      client.user_agent = request.custom_user_agent;
    }
    client.request_body_type = HTTPClientPOSIX::BodyFromFile;
    try {
      client.request_body_type = HTTPClientPOSIX::BodyFromBuffer;
      client.request_body_contents = ReadFileAsString(request.file_name);
      client.request_body_content_type = request.content_type;
    } catch (FileException&) {
      // TODO(dkorolev): Unfix this "fix" once we use proper exceptions in other clients (Apple, Android).
      throw HTTPClientException();
    }
    client.request_body_content_type = request.content_type;
  }

  inline static void PrepareInput(const KeepResponseInMemory&, HTTPClientPOSIX& client) {
    client.destination_type = HTTPClientPOSIX::DestinationBuffer;
  }

  inline static void PrepareInput(const SaveResponseToFile& save_to_file_request, HTTPClientPOSIX& client) {
    assert(!save_to_file_request.file_name.empty());
    client.destination_type = HTTPClientPOSIX::DestinationFile;
    client.destination_file_name = save_to_file_request.file_name;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& /*response_params*/,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponse& output) {
    if (request_params.url != response.url_requested) {
      throw HTTPClientException();
    }
    output.url = request_params.url;
    output.code = response.code_;
    output.url_after_redirects = response.url_after_redirects_;
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithBuffer& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    assert(response.destination_type == HTTPClientPOSIX::DestinationBuffer);
    const auto& message = response.message;
    output.body = message->HasBody() ? message->Body() : "";
  }

  template <typename T_REQUEST_PARAMS, typename T_RESPONSE_PARAMS>
  inline static void ParseOutput(const T_REQUEST_PARAMS& request_params,
                                 const T_RESPONSE_PARAMS& response_params,
                                 const HTTPClientPOSIX& response,
                                 HTTPResponseWithResultingFileName& output) {
    ParseOutput(request_params, response_params, response, static_cast<HTTPResponse&>(output));
    assert(response.destination_type == HTTPClientPOSIX::DestinationFile);
    // TODO(dkorolev): This is doubly inefficient. Gotta write the buffer or write chunks.
    const auto& message = response.message;
    WriteStringToFile(response_params.file_name, message->HasBody() ? message->Body() : "");
    output.body_file_name = response_params.file_name;
  }
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_POSIX_H
