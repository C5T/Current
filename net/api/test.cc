/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// TODO(dkorolev): Add a 404 test for downloading into file.

// Test for HTTP clients.
// Note that this test relies on HTTP server defined in Bricks.
// Thus, it might have to be tweaked on Windows. TODO(dkorolev): Do it.

#include <string>

// Test two HTTP server implementations: The one using pure HTTP code from Bricks and the one using Bricks API.
#include "test_server.cc.h"
#include "../http/test_server.cc.h"

#include "../../dflags/dflags.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

#include "api.h"

#include "../tcp/tcp.h"
#include "../url/url.h"

#include "../../file/file.h"
#include "../../port.h"
#include "../../util/make_scope_guard.h"

using std::string;

using bricks::FileSystem;
using bricks::FileException;

using bricks::net::HTTPRedirectNotAllowedException;
using bricks::net::HTTPRedirectLoopException;
using bricks::net::SocketResolveAddressException;

using bricks::net::http::test::TestHTTPServer_HTTPImpl;
using bricks::net::api::test::TestHTTPServer_APIImpl;

using namespace bricks::net::api;

#ifndef BRICKS_COVERAGE_REPORT_MODE
TEST(ArchitectureTest, BRICKS_ARCH_UNAME_AS_IDENTIFIER) {
  ASSERT_EQ(BRICKS_ARCH_UNAME, FLAGS_bricks_runtime_arch);
}
#endif

DEFINE_int32(net_api_test_port, 8080, "Local port to use for the test HTTP server.");
DEFINE_string(net_api_test_tmpdir, ".noshit", "Local path for the test to create temporary files in.");

struct TestHTTPServer_httpbin_org {
  struct DummyTypeWithNonTrivialDestructor {
    string BaseURL() const { return "http://httpbin.org"; }
    ~DummyTypeWithNonTrivialDestructor() {
      // To avoid the `unused variable: server_scope` warning down in the tests.
    }
  };
  static DummyTypeWithNonTrivialDestructor Spawn(int) { return DummyTypeWithNonTrivialDestructor{}; }
};

template <typename T>
class HTTPClientTemplatedTest : public ::testing::Test {};

typedef ::testing::Types<TestHTTPServer_HTTPImpl,
                         TestHTTPServer_APIImpl
#ifndef BRICKS_COVERAGE_REPORT_MODE
                         ,
                         TestHTTPServer_httpbin_org  // This implementation will fail the redirect loop test.
#endif
                         > TestHTTPServerTypesList;
TYPED_TEST_CASE(HTTPClientTemplatedTest, TestHTTPServerTypesList);

TYPED_TEST(HTTPClientTemplatedTest, GetToBuffer) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/drip?numbytes=7";
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("*******", response.body);
  EXPECT_EQ(url, response.url);
}

TYPED_TEST(HTTPClientTemplatedTest, GetToFile) {
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_test_file_for_http_get";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/drip?numbytes=5";
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ("*****", FileSystem::ReadFileAsString(response.body_file_name));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromBufferToBuffer) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/post";
  const string post_body = "Hello, World!";
  const auto response = HTTP(POST(url, post_body, "application/octet-stream"));
  EXPECT_NE(string::npos, response.body.find("\"data\": \"" + post_body + "\"")) << response.body;
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromInvalidFile) {
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/post";
  const string non_existent_file_name = FLAGS_net_api_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(non_existent_file_name);
  ASSERT_THROW(HTTP(POSTFromFile(url, non_existent_file_name, "text/plain")), FileException);
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromFileToBuffer) {
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/post";
  FileSystem::WriteStringToFile(file_name.c_str(), file_name);
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_NE(string::npos, response.body.find(file_name));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromBufferToFile) {
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/post";
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_NE(string::npos, FileSystem::ReadFileAsString(response.body_file_name).find("TEST BODY"));
}

TYPED_TEST(HTTPClientTemplatedTest, PostFromFileToFile) {
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string request_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name =
      FLAGS_net_api_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = FileSystem::ScopedRemoveFile(request_file_name);
  const auto output_file_scope = FileSystem::ScopedRemoveFile(response_file_name);
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/post";
  const string post_body = "Aloha, this text should pass from one file to another. Mahalo!";
  FileSystem::WriteStringToFile(request_file_name.c_str(), post_body);
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  {
    const string received_data = FileSystem::ReadFileAsString(response.body_file_name);
    EXPECT_NE(string::npos, received_data.find(post_body)) << received_data << "\n" << post_body;
  }
}

TYPED_TEST(HTTPClientTemplatedTest, ErrorCodes) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/status/403";
  EXPECT_EQ(403, static_cast<int>(HTTP(GET(url)).code));
}

TYPED_TEST(HTTPClientTemplatedTest, SendsURLParameters) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/get?Aloha=Mahalo";
  const auto response = HTTP(GET(url));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_NE(string::npos, response.body.find("\"Aloha\": \"Mahalo\""));
}

TYPED_TEST(HTTPClientTemplatedTest, HttpRedirect302NotAllowedByDefault) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  ASSERT_THROW(HTTP(GET(server_scope.BaseURL() + "/redirect-to?url=/get")), HTTPRedirectNotAllowedException);
}

TYPED_TEST(HTTPClientTemplatedTest, HttpRedirect302Allowed) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/redirect-to?url=/get";
  const auto response = HTTP(GET(url).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_NE(url, response.url);
  EXPECT_EQ(server_scope.BaseURL() + "/get", response.url);
}

TYPED_TEST(HTTPClientTemplatedTest, UserAgent) {
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  const string url = server_scope.BaseURL() + "/user-agent";
  const string custom_user_agent = "Aloha User Agent";
  const auto response = HTTP(GET(url).UserAgent(custom_user_agent));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_NE(string::npos, response.body.find(custom_user_agent));
}

TYPED_TEST(HTTPClientTemplatedTest, InvalidUrl) {
  ASSERT_THROW(HTTP(GET("http://999.999.999.999/")), SocketResolveAddressException);
}

TYPED_TEST(HTTPClientTemplatedTest, RedirectLoopWillFailOnHttpBinOrg) {
  // This test will fail on httpbin.org, which is perfectly fine, since test suite using httpbin.org
  // is not run when all the tests are run in batch.
  const auto server_scope = TypeParam::Spawn(FLAGS_net_api_test_port);
  ASSERT_THROW(HTTP(GET(server_scope.BaseURL() + "/redirect-loop").AllowRedirects()),
               HTTPRedirectLoopException);
}
