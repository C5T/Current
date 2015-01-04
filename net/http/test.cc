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

#include <thread>

#include "http.h"

#include "../../dflags/dflags.h"

#include "../../3party/gtest/gtest.h"
#include "../../3party/gtest/gtest-main-with-dflags.h"

#include "../../strings/printf.h"

DEFINE_int32(net_http_test_port, 8082, "Local port to use for the test server.");

using std::string;
using std::thread;
using std::to_string;

using namespace bricks;

using bricks::net::Socket;
using bricks::net::ClientSocket;
using bricks::net::Connection;
using bricks::net::HTTPServerConnection;
using bricks::net::HTTPReceivedMessage;
using bricks::net::HTTPResponseCode;
using bricks::net::HTTPResponseCodeAsStringGenerator;
using bricks::net::HTTPNoBodyProvidedException;

struct HTTPClientImplCURL {
  static string Syscall(const string& cmdline) {
    FILE* pipe = ::popen(cmdline.c_str(), "r");
    assert(pipe);
    char s[1024];
    ::fgets(s, sizeof(s), pipe);
    ::pclose(pipe);
    return s;
  }

  static string Fetch(thread& server_thread, const string& url, const string& method) {
    const string result = Syscall(
        strings::Printf("curl -s -X %s localhost:%d%s", method.c_str(), FLAGS_net_http_test_port, url.c_str()));
    server_thread.join();
    return result;
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    const string result = Syscall(strings::Printf("curl -s -X %s -d '%s' localhost:%d%s",
                                                  method.c_str(),
                                                  data.c_str(),
                                                  FLAGS_net_http_test_port,
                                                  url.c_str()));
    server_thread.join();
    return result;
  }
};

class HTTPClientImplPOSIX {
 public:
  static string Fetch(thread& server_thread, const string& url, const string& method) {
    return Impl(server_thread, url, method);
  }

  static string FetchWithBody(thread& server_thread,
                              const string& url,
                              const string& method,
                              const string& data) {
    return Impl(server_thread, url, method, true, data);
  }

 private:
  static string Impl(thread& server_thread,
                     const string& url,
                     const string& method,
                     bool has_data = false,
                     const string& data = "") {
    Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
    connection.BlockingWrite(method + ' ' + url + "\r\n");
    if (has_data) {
      connection.BlockingWrite("Content-Length: " + to_string(data.length()) + "\r\n");
    }
    connection.BlockingWrite("\r\n");
    if (has_data) {
      connection.BlockingWrite(data);
    }
    connection.SendEOF();
    HTTPReceivedMessage message(connection);
    assert(message.HasBody());
    const string body = message.Body();
    server_thread.join();
    return body;
  }
};

template <typename T>
class HTTPTest : public ::testing::Test {};

typedef ::testing::Types<HTTPClientImplPOSIX, HTTPClientImplCURL> HTTPClientImplsTypeList;
TYPED_TEST_CASE(HTTPTest, HTTPClientImplsTypeList);

TYPED_TEST(HTTPTest, GET) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("GET", c.Message().Method());
             EXPECT_EQ("/unittest", c.Message().URL());
             c.SendHTTPResponse("PASSED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/unittest_post", c.Message().URL());
             ASSERT_TRUE(c.Message().HasBody()) << "WTF!";
             EXPECT_EQ("BAZINGA", c.Message().Body());
             c.SendHTTPResponse("POSTED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/unittest_empty_post", c.Message().URL());
             EXPECT_FALSE(c.Message().HasBody());
             ASSERT_THROW(c.Message().Body(), HTTPNoBodyProvidedException);
             c.SendHTTPResponse("ALMOST_POSTED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("ALMOST_POSTED", TypeParam::Fetch(t, "/unittest_empty_post", "POST"));
}

TEST(HTTPCodesTest, Code200) {
  EXPECT_EQ("OK", HTTPResponseCodeAsStringGenerator::CodeAsString(static_cast<HTTPResponseCode>(200)));
}

TEST(HTTPCodesTest, Code404) {
  EXPECT_EQ("Not Found", HTTPResponseCodeAsStringGenerator::CodeAsString(static_cast<HTTPResponseCode>(404)));
}

TEST(HTTPCodesTest, CodeUnknown) {
  EXPECT_EQ("Unknown Code",
            HTTPResponseCodeAsStringGenerator::CodeAsString(static_cast<HTTPResponseCode>(999)));
}

TEST(HTTPCodesTest, CodeInternalUninitialized) {
  EXPECT_EQ("<UNINITIALIZED>",
            HTTPResponseCodeAsStringGenerator::CodeAsString(static_cast<HTTPResponseCode>(-1)));
}
