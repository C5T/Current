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
using bricks::net::ConnectionResetByPeer;

TEST(PosixHTTPServerTest, Smoke) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/", c.Message().Path());
             c.SendHTTPResponse("Data: " + c.Message().Body());
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 4\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("BODY");
  // The last "\r\n" and EOF are unnecessary, but conventional here. See the test below w/o them.
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "Data: BODY\r\n",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, NoEOF) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/", c.Message().Path());
             c.SendHTTPResponse("Data: " + c.Message().Body());
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 5\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("NOEOF");
  // Do not send the last "\r\n" and EOF. See the test above with them.
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 11\r\n"
      "\r\n"
      "Data: NOEOF\r\n",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, LargeBody) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/", c.Message().Path());
             c.SendHTTPResponse("Data: " + c.Message().Body());
           },
           Socket(FLAGS_net_http_test_port));
  string body(1000000, '.');
  for (size_t i = 0; i < 1000000; ++i) {
    body[i] = 'A' + (i % 26);
  }
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 1000000\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite(body);
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 1000006\r\n"
      "\r\n"
      "Data: " +
          body + "\r\n",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, ChunkedLargeBodyManyChunks) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/", c.Message().Path());
             c.SendHTTPResponse(c.Message().Body());
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n");
  connection.BlockingWrite("\r\n");
  string chunk(10, '.');
  string body = "";
  for (size_t i = 0; i < 10000; ++i) {
    connection.BlockingWrite("A\r\n");  // "A" is hexadecimal for 10.
    for (size_t j = 0; j < 10; ++j) {
      chunk[j] = 'A' + ((i + j) % 26);
    }
    connection.BlockingWrite(chunk);
    body += chunk;
    connection.BlockingWrite("\r\n");
  }
  connection.BlockingWrite("0\r\n");
  t.join();
  EXPECT_EQ(strings::Printf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s"
                "\r\n",
                static_cast<int>(body.length()),
                body.c_str()),
            connection.BlockingReadUntilEOF());
}

// A dedicated test to cover buffer resize after the size of the next chunk has been received.
TEST(PosixHTTPServerTest, ChunkedBodyLargeFirstChunk) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/", c.Message().Path());
             c.SendHTTPResponse(c.Message().Body());
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n");
  connection.BlockingWrite("\r\n");
  string chunk(10000, '.');
  string body = "";
  for (size_t i = 0; i < 10; ++i) {
    connection.BlockingWrite(strings::Printf("%X\r\n", 10000));
    for (size_t j = 0; j < 10000; ++j) {
      chunk[j] = 'a' + ((i + j) % 26);
    }
    connection.BlockingWrite(chunk);
    body += chunk;
    connection.BlockingWrite("\r\n");
  }
  connection.BlockingWrite("0\r\n");
  t.join();
  EXPECT_EQ(strings::Printf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s"
                "\r\n",
                static_cast<int>(body.length()),
                body.c_str()),
            connection.BlockingReadUntilEOF());
}

TEST(HTTPServerTest, ConnectionResetByPeerException) {
  bool connection_reset_by_peer = false;
  thread t([&connection_reset_by_peer](Socket s) {
             try {
               HTTPServerConnection(s.Accept());
             } catch (const ConnectionResetByPeer&) {
               connection_reset_by_peer = true;
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 1000000\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("This body message terminates prematurely.\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_TRUE(connection_reset_by_peer);
}

// A dedicated test to cover the `ConnectionResetByPeer` case while receiving a chunk.
TEST(PosixHTTPServerTest, ChunkedBodyConnectionResetByPeerException) {
  bool connection_reset_by_peer = false;
  thread t([&connection_reset_by_peer](Socket s) {
             try {
               HTTPServerConnection(s.Accept());
             } catch (const ConnectionResetByPeer&) {
               connection_reset_by_peer = true;
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("10000\r\n");
  connection.BlockingWrite("This body message terminates prematurely.\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_TRUE(connection_reset_by_peer);
}

struct HTTPClientImplCURL {
  static string Syscall(const string& cmdline) {
    FILE* pipe = ::popen(cmdline.c_str(), "r");
    assert(pipe);
    char s[1024];
    const auto warn_unused_result_catch_warning = ::fgets(s, sizeof(s), pipe);
    static_cast<void>(warn_unused_result_catch_warning);
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
             EXPECT_EQ("/unittest", c.Message().Path());
             c.SendHTTPResponse("PASSED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.Message().Method());
             EXPECT_EQ("/unittest_post", c.Message().Path());
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
             EXPECT_EQ("/unittest_empty_post", c.Message().Path());
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
