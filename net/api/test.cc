// TODO(dkorolev): Add a 404 test for downloading into file.

#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <functional>

#include "http_client.h"

#include "../http.h"

#include "../../util/make_scope_guard.h"
#include "../../file/file.h"

#include "../../dflags/dflags.h"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main.h"

using std::function;
using std::ifstream;
using std::move;
using std::ofstream;
using std::string;
using std::thread;
using std::to_string;

using aloha::HTTPClientPlatformWrapper;

using bricks::ScopeGuard;
using bricks::MakeScopeGuard;

using bricks::ReadFileAsString;

using bricks::net::Socket;
using bricks::net::HTTPConnection;
using bricks::net::HTTPHeadersType;
using bricks::net::HTTPResponseCode;

DEFINE_int32(port, 8080, "Local port to use for the test HTTP server.");
DEFINE_string(test_tmpdir, "build", "Local path for the test to create temporary files in.");

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

// TODO(dkorolev): Merge exception types into bricks/net.
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

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

class UseHTTPBinTestServer {
 public:
  struct DummyTypeWithNonTrivialDestructor {
    ~DummyTypeWithNonTrivialDestructor() {
      // To avoid the `unused variable: server_scope` warning down in the tests.
    }
  };
  static string BaseURL() {
    return "http://httpbin.org";
  }
  static DummyTypeWithNonTrivialDestructor SpawnServer() {
    return DummyTypeWithNonTrivialDestructor{};
  }

  // TODO(dkorolev): Get rid of this.
  static bool SupportsExternalURLs() {
    return true;
  }
};

class UseLocalHTTPTestServer {
 public:
  static string BaseURL() {
    return string("http://localhost:") + to_string(FLAGS_port);
  }

  class ThreadForSingleServerRequest {
   public:
    ThreadForSingleServerRequest(function<void(Socket)> server_impl)
        : server_thread_(server_impl, std::move(Socket(FLAGS_port))) {
    }
    ThreadForSingleServerRequest(ThreadForSingleServerRequest&& rhs)
        : server_thread_(std::move(rhs.server_thread_)) {
    }
    ~ThreadForSingleServerRequest() {
      server_thread_.join();
    }

   private:
    thread server_thread_;
  };

  static ThreadForSingleServerRequest SpawnServer() {
    return ThreadForSingleServerRequest(UseLocalHTTPTestServer::TestServerHandler);
  }

  // TODO(dkorolev): Get rid of this.
  static bool SupportsExternalURLs() {
    return false;
  }

 private:
  // TODO(dkorolev): This code should use our real bricks::HTTPServer, once it's coded.
  static void TestServerHandler(Socket socket) {
    bool serve_more_requests = true;
    while (serve_more_requests) {
      serve_more_requests = false;
      HTTPConnection connection(socket.Accept());
      ASSERT_TRUE(connection);
      const string method = connection.Method();
      const string url = connection.URL();
      if (method == "GET") {
        if (url == "/get") {
          connection.SendHTTPResponse("DIMA");
        } else if (url == "/drip?numbytes=7") {
          connection.SendHTTPResponse("*******");
        } else if (url == "/drip?numbytes=5") {
          connection.SendHTTPResponse("*****");
        } else if (url == "/status/403") {
          connection.SendHTTPResponse("", HTTPResponseCode::Forbidden);
        } else if (url == "/get?Aloha=Mahalo") {
          connection.SendHTTPResponse("{\"Aloha\": \"Mahalo\"}");
        } else if (url == "/user-agent") {
          // TODO(dkorolev): Add parsing User-Agent to Bricks' HTTP headers parser.
          connection.SendHTTPResponse("Aloha User Agent");
        } else if (url == "/redirect-to?url=/get") {
          HTTPHeadersType headers;
          headers.push_back(std::make_pair("Location", "/get"));
          connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
          serve_more_requests = true;
        } else {
          ASSERT_TRUE(false) << "GET not implemented for: " << connection.URL();
        }
      } else if (method == "POST") {
        if (url == "/post") {
          ASSERT_TRUE(connection.HasBody());
          connection.SendHTTPResponse("\"data\": \"" + connection.Body() + "\"");
        } else {
          ASSERT_TRUE(false) << "POST not implemented for: " << connection.URL();
        }
      } else {
        ASSERT_TRUE(false) << "Method not implemented: " << connection.Method();
      }
    }
  }
};

static void WriteStringToFile(const string& file_name, const string& contents) {
  ofstream file(file_name);
  file << contents;
  ASSERT_TRUE(file.good());
}

static const auto ScopedFileCleanup = [](const string& file_name) {
  ::remove(file_name.c_str());
  return move(MakeScopeGuard([&] { ::remove(file_name.c_str()); }));
};

template <typename T>
class HTTPClientTemplatedTest : public ::testing::Test {};

typedef ::testing::Types<UseLocalHTTPTestServer, UseHTTPBinTestServer> HTTPClientTestTypeList;
TYPED_TEST_CASE(HTTPClientTemplatedTest, HTTPClientTestTypeList);

TYPED_TEST(HTTPClientTemplatedTest, GetToBuffer) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/drip?numbytes=7";
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, response.code);
  EXPECT_EQ("*******", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(url, response.url_after_redirects);
  EXPECT_FALSE(response.was_redirected);
}

TYPED_TEST(HTTPClientTemplatedTest, GetToFile) {
  const string file_name = FLAGS_test_tmpdir + "/some_test_file_for_http_get";
  const auto test_file_scope = ScopedFileCleanup(file_name);
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/drip?numbytes=5";
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, response.code);
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(url, response.url_after_redirects);
  EXPECT_FALSE(response.was_redirected);
  EXPECT_EQ("*****", ReadFileAsString(response.body_file_name));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromBufferToBuffer) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/post";
  const string post_body = "Hello, World!";
  const auto response = HTTP(POST(url, post_body, "application/octet-stream"));
  EXPECT_NE(string::npos, response.body.find("\"data\": \"" + post_body + "\"")) << response.body;
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromInvalidFile) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/post";
  const string non_existent_file_name = FLAGS_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = ScopedFileCleanup(non_existent_file_name);
  ASSERT_THROW(HTTP(POSTFromFile(url, non_existent_file_name, "text/plain")), HTTPClientException);
  // Still do one request since local HTTP server is waiting for it.
  EXPECT_EQ(200, HTTP(GET(TypeParam::BaseURL() + "/get")).code);
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromFileToBuffer) {
  const string file_name = FLAGS_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = ScopedFileCleanup(file_name);
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/post";
  WriteStringToFile(file_name, file_name);
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, response.code);
  EXPECT_NE(string::npos, response.body.find(file_name));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromBufferToFile) {
  const string file_name = FLAGS_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = ScopedFileCleanup(file_name);
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/post";
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, response.code);
  EXPECT_NE(string::npos, ReadFileAsString(response.body_file_name).find("TEST BODY"));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromFileToFile) {
  const string request_file_name = FLAGS_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name = FLAGS_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = ScopedFileCleanup(request_file_name);
  const auto output_file_scope = ScopedFileCleanup(response_file_name);
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/post";
  const string post_body = "Aloha, this text should pass from one file to another. Mahalo!";
  WriteStringToFile(request_file_name, post_body);
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, response.code);
  {
    const string received_data = ReadFileAsString(response.body_file_name);
    EXPECT_NE(string::npos, received_data.find(post_body)) << received_data << "\n" << post_body;
  }
}

TYPED_TEST(HTTPClientTemplatedTest, ErrorCodes) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/status/403";
  EXPECT_EQ(403, HTTP(GET(url)).code);
}

TYPED_TEST(HTTPClientTemplatedTest, Https) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/get?Aloha=Mahalo";
  const auto response = HTTP(GET(url));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, response.code);
  EXPECT_FALSE(response.was_redirected);
  EXPECT_NE(string::npos, response.body.find("\"Aloha\": \"Mahalo\""));
}

TYPED_TEST(HTTPClientTemplatedTest, HttpRedirect302) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/redirect-to?url=/get";
  const auto response = HTTP(GET(url));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, response.code);
  EXPECT_EQ(TypeParam::BaseURL() + "/get", response.url_after_redirects);
  EXPECT_TRUE(response.was_redirected);
}

TYPED_TEST(HTTPClientTemplatedTest, UserAgent) {
  const auto server_scope = TypeParam::SpawnServer();
  const string url = TypeParam::BaseURL() + "/user-agent";
  const string custom_user_agent = "Aloha User Agent";
  const auto response = HTTP(GET(url).SetUserAgent(custom_user_agent));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, response.code);
  EXPECT_NE(string::npos, response.body.find(custom_user_agent));
}

// TODO(dkorolev): Get rid of the tests involving external URLs.
TYPED_TEST(HTTPClientTemplatedTest, HttpRedirect301) {
  if (TypeParam::SupportsExternalURLs()) {
    const auto response = HTTP(GET("http://github.com"));
    EXPECT_EQ(200, response.code);
    EXPECT_TRUE(response.was_redirected);
    EXPECT_EQ("https://github.com/", response.url_after_redirects);
  }
}

TYPED_TEST(HTTPClientTemplatedTest, HttpRedirect307) {
  if (TypeParam::SupportsExternalURLs()) {
    const auto response = HTTP(GET("http://msn.com"));
    EXPECT_EQ(200, response.code);
    EXPECT_TRUE(response.was_redirected);
    EXPECT_EQ("http://www.msn.com/", response.url_after_redirects);
  }
}

TYPED_TEST(HTTPClientTemplatedTest, InvalidUrl) {
  if (TypeParam::SupportsExternalURLs()) {
    const auto response = HTTP(GET("http://very.bad.url/that/will/not/load"));
    EXPECT_NE(200, response.code);
  }
}
