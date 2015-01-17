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

// TODO(dkorolev): Add code and test for the "/id/:id/value" type of URL.

#include <string>

#include "../../dflags/dflags.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

#include "api.h"
#include "../url/url.h"

#include "../../port.h"
#include "../../strings/printf.h"
#include "../../file/file.h"

using std::string;

using bricks::strings::Printf;
using bricks::FileSystem;
using bricks::FileException;

using bricks::net::Connection;
using bricks::net::HTTPResponseCode;
using bricks::net::HTTPHeadersType;

using bricks::net::HTTPRedirectNotAllowedException;
using bricks::net::HTTPRedirectLoopException;
using bricks::net::SocketResolveAddressException;

using namespace bricks::net::api;

DEFINE_int32(net_api_test_port,
             8082,
             "Local port to use for the test API-based HTTP server. NOTE: This port should be different from "
             "ports in other network-based tests, since API-driven HTTP server will hold it open for the whole "
             "lifetime of the binary.");
DEFINE_string(net_api_test_tmpdir, ".noshit", "Local path for the test to create temporary files in.");

#ifndef BRICKS_COVERAGE_REPORT_MODE
TEST(ArchitectureTest, BRICKS_ARCH_UNAME_AS_IDENTIFIER) {
  ASSERT_EQ(BRICKS_ARCH_UNAME, FLAGS_bricks_runtime_arch);
}
#endif

TEST(HTTPAPI, Register) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/get", [](Request&& r) { r.connection.SendHTTPResponse("OK"); });
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/get", [](Request&&) {}), HandlerAlreadyExistsException);
  const string url = Printf("localhost:%d/get", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("OK", response.body);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
}

TEST(HTTPAPI, UnRegister) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request&& r) { r.connection.SendHTTPResponse("bar"); });
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).Register("/foo", [](Request&&) {}), HandlerAlreadyExistsException);
  const string url = Printf("localhost:%d/foo", FLAGS_net_api_test_port);
  EXPECT_EQ(200, static_cast<int>(HTTP(GET(url)).code));
  EXPECT_EQ(1u, HTTP(FLAGS_net_api_test_port).HandlersCount());
  HTTP(FLAGS_net_api_test_port).UnRegister("/foo");
  EXPECT_EQ(404, static_cast<int>(HTTP(GET(url)).code));
  EXPECT_EQ(0u, HTTP(FLAGS_net_api_test_port).HandlersCount());
  ASSERT_THROW(HTTP(FLAGS_net_api_test_port).UnRegister("/foo"), HandlerDoesNotExistException);
}

TEST(HTTPAPI, URLParameters) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port)
      .Register("/query", [](Request&& r) { r.connection.SendHTTPResponse("x=" + r.url.query["x"]); });
  EXPECT_EQ("x=", HTTP(GET(Printf("localhost:%d/query", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=42", HTTP(GET(Printf("localhost:%d/query?x=42", FLAGS_net_api_test_port))).body);
  EXPECT_EQ("x=test passed",
            HTTP(GET(Printf("localhost:%d/query?x=test+passed", FLAGS_net_api_test_port))).body);
}

TEST(HTTPAPI, Redirect) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/from", [](Request&& r) {
    r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", {{"Location", "/to"}});
  });
  HTTP(FLAGS_net_api_test_port).Register("/to", [](Request&& r) { r.connection.SendHTTPResponse("Done."); });
  // Redirect not allowed by default.
  ASSERT_THROW(HTTP(GET(Printf("localhost:%d/from", FLAGS_net_api_test_port))),
               HTTPRedirectNotAllowedException);
  // Redirect allowed when `.AllowRedirects()` is set.
  const auto response = HTTP(GET(Printf("localhost:%d/from", FLAGS_net_api_test_port)).AllowRedirects());
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Done.", response.body);
  EXPECT_EQ(Printf("http://localhost:%d/to", FLAGS_net_api_test_port), response.url);
}

TEST(HTTPAPI, RedirectLoop) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/p1", [](Request&& r) {
    r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", {{"Location", "/p2"}});
  });
  HTTP(FLAGS_net_api_test_port).Register("/p2", [](Request&& r) {
    r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", {{"Location", "/p3"}});
  });
  HTTP(FLAGS_net_api_test_port).Register("/p3", [](Request&& r) {
    r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", {{"Location", "/p1"}});
  });
  ASSERT_THROW(HTTP(GET(Printf("localhost:%d/p1", FLAGS_net_api_test_port))), HTTPRedirectLoopException);
}

TEST(HTTPAPI, FourOhFour) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  const string url = Printf("localhost:%d/ORLY", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url));
  EXPECT_EQ(404, static_cast<int>(response.code));
  EXPECT_EQ("", response.body);
  EXPECT_EQ(url, response.url);
}

TEST(HTTPAPI, HandlerPreservesObject) {
  // Ensures that handler preserves the passed in object and does not make a copy of it.
  // It would be undesired if the handler object is copied, hence this test.
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  struct Helper {
    size_t counter = 0u;
    void operator()(Request&& r) {
      ++counter;
      r.connection.SendHTTPResponse("Incremented.");
    }
  };
  Helper helper;
  Helper copy(helper);
  EXPECT_EQ(0u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  HTTP(FLAGS_net_api_test_port).Register("/incr", helper);
  HTTP(FLAGS_net_api_test_port).Register("/incr_same", helper);
  HTTP(FLAGS_net_api_test_port).Register("/incr_copy", copy);
  EXPECT_EQ("Incremented.", HTTP(GET(Printf("localhost:%d/incr", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(1u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented.", HTTP(GET(Printf("localhost:%d/incr_same", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(0u, copy.counter);
  EXPECT_EQ("Incremented.", HTTP(GET(Printf("localhost:%d/incr_copy", FLAGS_net_api_test_port))).body);
  EXPECT_EQ(2u, helper.counter);
  EXPECT_EQ(1u, copy.counter);
}

TEST(HTTPAPI, GetToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/stars", [](Request&& r) {
    const uint64_t kDelayBetweenChunksInMilliseconds = 10;
    const size_t n = atoi(r.url.query["n"].c_str());
    // TODO(dkorolev): Refactor chunked response generation as a method of HTTPConnection.
    Connection& c = r.connection.RawConnection();
    // Send in some extra, RFC-allowed, whitespaces.
    c.BlockingWrite("HTTP/1.1  \t  \t\t  200\t    \t\t\t\t  OK\r\n");
    c.BlockingWrite("Transfer-Encoding: chunked\r\n");
    c.BlockingWrite("Content-Type: application/octet-stream\r\n");
    c.BlockingWrite("\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(kDelayBetweenChunksInMilliseconds));
    for (size_t i = 0; i < n; ++i) {
      c.BlockingWrite("1\r\n*\r\n");
      std::this_thread::sleep_for(std::chrono::milliseconds(kDelayBetweenChunksInMilliseconds));
    }
    c.BlockingWrite("0\r\n\r\n");  // Line ending as httpbin.org seems to do it. -- D.K.
    c.SendEOF();
  });
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_test_file_for_http_get";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const string url = Printf("localhost:%d/stars?n=3", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ(file_name, response.body_file_name);
  EXPECT_EQ(url, response.url);
  EXPECT_EQ("***", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, PostFromBufferToBuffer) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request&& r) {
    ASSERT_TRUE(r.message.HasBody());
    r.connection.SendHTTPResponse("Data: " + r.message.Body());
  });
  const auto response =
      HTTP(POST(Printf("localhost:%d/post", FLAGS_net_api_test_port), "No shit!", "application/octet-stream"));
  EXPECT_EQ("Data: No shit!", response.body);
}

TEST(HTTPAPI, PostFromInvalidFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string non_existent_file_name = FLAGS_net_api_test_tmpdir + "/non_existent_file";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(non_existent_file_name);
  ASSERT_THROW(HTTP(POSTFromFile(
                   Printf("localhost:%d/foo", FLAGS_net_api_test_port), non_existent_file_name, "text/plain")),
               FileException);
}

TEST(HTTPAPI, PostFromFileToBuffer) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request&& r) {
    ASSERT_TRUE(r.message.HasBody());
    r.connection.SendHTTPResponse("Voila: " + r.message.Body());
  });
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_input_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const string url = Printf("localhost:%d/post", FLAGS_net_api_test_port);
  FileSystem::WriteStringToFile(file_name.c_str(), "No shit detected.");
  const auto response = HTTP(POSTFromFile(url, file_name, "application/octet-stream"));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Voila: No shit detected.", response.body);
}

TEST(HTTPAPI, PostFromBufferToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request&& r) {
    ASSERT_TRUE(r.message.HasBody());
    r.connection.SendHTTPResponse("Meh: " + r.message.Body());
  });
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string file_name = FLAGS_net_api_test_tmpdir + "/some_output_test_file_for_http_post";
  const auto test_file_scope = FileSystem::ScopedRemoveFile(file_name);
  const string url = Printf("localhost:%d/post", FLAGS_net_api_test_port);
  const auto response = HTTP(POST(url, "TEST BODY", "text/plain"), SaveResponseToFile(file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Meh: TEST BODY", FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, PostFromFileToFile) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/post", [](Request&& r) {
    ASSERT_TRUE(r.message.HasBody());
    r.connection.SendHTTPResponse("Phew: " + r.message.Body());
  });
  bricks::FileSystem::CreateDirectory(FLAGS_net_api_test_tmpdir, FileSystem::CreateDirectoryParameters::Silent);
  const string request_file_name = FLAGS_net_api_test_tmpdir + "/some_complex_request_test_file_for_http_post";
  const string response_file_name =
      FLAGS_net_api_test_tmpdir + "/some_complex_response_test_file_for_http_post";
  const auto input_file_scope = FileSystem::ScopedRemoveFile(request_file_name);
  const auto output_file_scope = FileSystem::ScopedRemoveFile(response_file_name);
  const string url = Printf("localhost:%d/post", FLAGS_net_api_test_port);
  const string post_body = "Aloha, this text should pass from one file to another. Mahalo!";
  FileSystem::WriteStringToFile(request_file_name.c_str(), post_body);
  const auto response =
      HTTP(POSTFromFile(url, request_file_name, "text/plain"), SaveResponseToFile(response_file_name));
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("Phew: Aloha, this text should pass from one file to another. Mahalo!",
            FileSystem::ReadFileAsString(response.body_file_name));
}

TEST(HTTPAPI, UserAgent) {
  HTTP(FLAGS_net_api_test_port).ResetAllHandlers();
  HTTP(FLAGS_net_api_test_port).Register("/ua", [](Request&& r) {
    r.connection.SendHTTPResponse("TODO(dkorolev): Actually get passed in user agent.");
  });
  const string url = Printf("localhost:%d/ua", FLAGS_net_api_test_port);
  const auto response = HTTP(GET(url).UserAgent("Aloha"));
  EXPECT_EQ(url, response.url);
  EXPECT_EQ(200, static_cast<int>(response.code));
  EXPECT_EQ("TODO(dkorolev): Actually get passed in user agent.", response.body);
}

TEST(HTTPAPI, InvalidUrl) { ASSERT_THROW(HTTP(GET("http://999.999.999.999/")), SocketResolveAddressException); }
