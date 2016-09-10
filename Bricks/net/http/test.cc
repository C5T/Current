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

#include "../../port.h"

#include "headers/test.cc"

#include <thread>

#include "http.h"

#include "../../dflags/dflags.h"

#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

#include "../../strings/printf.h"

DEFINE_int32(net_http_test_port, PickPortForUnitTest(), "Local port to use for the test HTTP server.");

using std::string;
using std::thread;
using std::to_string;

using current::net::Socket;
using current::net::ClientSocket;
using current::net::Connection;
using current::net::HTTPServerConnection;
using current::net::HTTPRequestData;
using current::net::HTTPResponseCodeValue;
using current::net::HTTPResponseCodeAsString;
using current::net::GetFileMimeType;
using current::net::DefaultInternalServerErrorMessage;
using current::net::SocketException;
using current::net::ConnectionResetByPeer;
using current::net::AttemptedToSendHTTPResponseMoreThanOnce;

static void ExpectToReceive(const std::string& golden, Connection& connection) {
  std::vector<char> response(golden.length());
  try {
    ASSERT_EQ(golden.length(), connection.BlockingRead(&response[0], golden.length(), Connection::FillFullBuffer));
    EXPECT_EQ(golden, std::string(response.begin(), response.end()));
  } catch (const SocketException& e) {  // LCOV_EXCL_LINE
    ASSERT_TRUE(false) << e.What();     // LCOV_EXCL_LINE
  }
}

CURRENT_STRUCT(HTTPTestObject) {
  CURRENT_FIELD(number, int);
  CURRENT_FIELD(text, std::string);
  CURRENT_FIELD(array, std::vector<int>);
  CURRENT_DEFAULT_CONSTRUCTOR(HTTPTestObject) : number(42), text("text"), array({1, 2, 3}) {}
};

TEST(PosixHTTPServerTest, Smoke) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
    EXPECT_EQ(FLAGS_net_http_test_port, c.LocalIPAndPort().port);
    EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
    EXPECT_LT(0, c.RemoteIPAndPort().port);
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/", c.HTTPRequest().RawPath());
    c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  EXPECT_EQ("127.0.0.1", connection.LocalIPAndPort().ip);
  EXPECT_LT(0, connection.LocalIPAndPort().port);
  EXPECT_EQ("127.0.0.1", connection.RemoteIPAndPort().ip);
  EXPECT_EQ(FLAGS_net_http_test_port, connection.RemoteIPAndPort().port);
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Content-Length: 4\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("BODY", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "Data: BODY",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithArray) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/vector_char", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(std::vector<char>({'S', 't', 'r', 'i', 'n', 'g'}));
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /vector_char HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 6\r\n"
      "\r\n"
      "String",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithObject) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/test_object", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(HTTPTestObject());
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /test_object HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Connection: close\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-Length: 44\r\n"
      "\r\n"
      "{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithNamedObject) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/test_named_object", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(HTTPTestObject(), "epic_object");
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /test_named_object HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Connection: close\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Content-Length: 60\r\n"
      "\r\n"
      "{\"epic_object\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeChunkedResponse) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/chunked", c.HTTPRequest().RawPath());
    auto r = c.SendChunkedHTTPResponse();
    r.Send("onetwothree");
    r.Send(std::vector<char>({'f', 'o', 'o'}));
    r.Send(HTTPTestObject());
    r.Send(HTTPTestObject(), "epic_chunk");
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /chunked HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Connection: keep-alive\r\n"
      "Access-Control-Allow-Origin: *\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "B\r\n"
      "onetwothree\r\n"
      "3\r\n"
      "foo\r\n"
      "2C\r\n"
      "{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}\n\r\n"
      "3B\r\n"
      "{\"epic_chunk\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n\r\n"
      "0\r\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithHeaders) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/header", c.HTTPRequest().RawPath());
    c.SendHTTPResponse("OK", HTTPResponseCode.OK, c.HTTPRequest().Body(), {{"foo", "bar"}, {"baz", "meh"}});
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /header HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Content-Length: 19\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("custom_content_type", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: custom_content_type\r\n"
      "Connection: close\r\n"
      "foo: bar\r\n"
      "baz: meh\r\n"
      "Content-Length: 2\r\n"
      "\r\n"
      "OK",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, LargeBody) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(std::string("Data: ") + c.HTTPRequest().Body());
  }, Socket(FLAGS_net_http_test_port));
  string body(1000000, '.');
  for (size_t i = 0; i < body.length(); ++i) {
    body[i] = 'A' + (i % 26);
  }
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite(strings::Printf("Content-Length: %d\r\n", static_cast<int>(body.length())), true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite(body, false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 1000006\r\n"
      "\r\n"
      "Data: " +
          body,
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, ChunkedLargeBodyManyChunks) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(c.HTTPRequest().Body());
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);
  string chunk(10, '.');
  string body = "";
  for (size_t i = 0; i < 10000; ++i) {
    connection.BlockingWrite("A\r\n", true);  // "A" is hexadecimal for 10.
    for (size_t j = 0; j < 10; ++j) {
      chunk[j] = 'A' + ((i + j) % 26);
    }
    connection.BlockingWrite(chunk, true);
    body += chunk;
    connection.BlockingWrite("\r\n", true);
  }
  connection.BlockingWrite("0\r\n", false);
  ExpectToReceive(strings::Printf(
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n",
                      static_cast<int>(body.length())) +
                      body,
                  connection);
  t.join();
}

// A dedicated test to cover buffer resize after the size of the next chunk has been received.
TEST(PosixHTTPServerTest, ChunkedBodyLargeFirstChunk) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(c.HTTPRequest().Body());
  }, Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);
  string chunk(10000, '.');
  string body = "";
  for (size_t i = 0; i < 10; ++i) {
    connection.BlockingWrite(strings::Printf("%X\r\n", 10000), true);
    for (size_t j = 0; j < 10000; ++j) {
      chunk[j] = 'a' + ((i + j) % 26);
    }
    connection.BlockingWrite(chunk, true);
    body += chunk;
  }
  connection.BlockingWrite("0\r\n", false);
  ExpectToReceive(strings::Printf(
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n",
                      static_cast<int>(body.length())) +
                      body,
                  connection);
  t.join();
}

#ifndef CURRENT_WINDOWS
struct HTTPClientImplCURL {
  static string Syscall(const string& cmdline) {
    FILE* pipe = ::popen(cmdline.c_str(), "r");
    CURRENT_ASSERT(pipe);
    char s[1024];
    const auto warn_unused_result_catch_warning = ::fgets(s, sizeof(s), pipe);
    static_cast<void>(warn_unused_result_catch_warning);
    ::pclose(pipe);
    return s;
  }

  static string Fetch(thread& server_thread, const string& url, const string& method) {
    const string result =
        Syscall(strings::Printf("curl -s -X %s localhost:%d%s", method.c_str(), FLAGS_net_http_test_port, url.c_str()));
    server_thread.join();
    return result;
  }

  static string FetchWithBody(thread& server_thread, const string& url, const string& method, const string& data) {
    const string result = Syscall(strings::Printf(
        "curl -s -X %s -d '%s' localhost:%d%s", method.c_str(), data.c_str(), FLAGS_net_http_test_port, url.c_str()));
    server_thread.join();
    return result;
  }
};
#endif

class HTTPClientImplPOSIX {
 public:
  static string Fetch(thread& server_thread, const string& url, const string& method) {
    return Impl(server_thread, url, method);
  }

  static string FetchWithBody(thread& server_thread, const string& url, const string& method, const string& data) {
    return Impl(server_thread, url, method, true, data);
  }

 private:
  static string Impl(
      thread& server_thread, const string& url, const string& method, bool has_data = false, const string& data = "") {
    Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
    connection.BlockingWrite(method + ' ' + url + "\r\n", true);
    if (has_data) {
      connection.BlockingWrite("Content-Length: " + to_string(data.length()) + "\r\n", true);
      connection.BlockingWrite("\r\n", true);
      connection.BlockingWrite(data, false);
    } else {
      connection.BlockingWrite("\r\n", false);
    }
    HTTPRequestData http_request(connection);
    CURRENT_ASSERT(!http_request.Body().empty());
    const string body = http_request.Body();
    server_thread.join();
    return body;
  }
};

template <typename T>
class HTTPTest : public ::testing::Test {};

#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE)
// Temporary disabled CURL tests for Apple - M.Z.
typedef ::testing::Types<HTTPClientImplPOSIX, HTTPClientImplCURL> HTTPClientImplsTypeList;
#else
typedef ::testing::Types<HTTPClientImplPOSIX> HTTPClientImplsTypeList;
#endif
TYPED_TEST_CASE(HTTPTest, HTTPClientImplsTypeList);

TYPED_TEST(HTTPTest, GET) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("GET", c.HTTPRequest().Method());
    EXPECT_EQ("/unittest?foo=bar", c.HTTPRequest().RawPath());
    EXPECT_EQ("/unittest", c.HTTPRequest().URL().path);
    EXPECT_EQ("bar", c.HTTPRequest().URL().query["foo"]);
    c.SendHTTPResponse("PASSED");
  }, Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest?foo=bar", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/unittest_post", c.HTTPRequest().RawPath());
    EXPECT_EQ(7u, c.HTTPRequest().BodyLength());
    EXPECT_EQ("BAZINGA", c.HTTPRequest().Body());
    c.SendHTTPResponse("POSTED");
  }, Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/unittest_empty_post", c.HTTPRequest().RawPath());
    EXPECT_EQ(0u, c.HTTPRequest().BodyLength());
    c.SendHTTPResponse("ALMOST_POSTED");
  }, Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("ALMOST_POSTED", TypeParam::Fetch(t, "/unittest_empty_post", "POST"));
}

TYPED_TEST(HTTPTest, AttemptsToSendResponseTwice) {
  thread t([](Socket s) {
    HTTPServerConnection c(s.Accept());
    c.SendHTTPResponse("one");
    ASSERT_THROW(c.SendHTTPResponse("two"), AttemptedToSendHTTPResponseMoreThanOnce);
    ASSERT_THROW(c.SendChunkedHTTPResponse().Send("three"), AttemptedToSendHTTPResponseMoreThanOnce);
  }, Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("one", TypeParam::Fetch(t, "/", "GET"));
}

TYPED_TEST(HTTPTest, DoesNotSendResponseAtAll) {
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", DefaultInternalServerErrorMessage());
  thread t([](Socket s) { HTTPServerConnection c(s.Accept()); }, Socket(FLAGS_net_http_test_port));
  EXPECT_EQ(DefaultInternalServerErrorMessage(), TypeParam::Fetch(t, "/", "GET"));
}

TEST(HTTPCodesTest, SmokeTest) {
  EXPECT_EQ("OK", HTTPResponseCodeAsString(HTTPResponseCode(200)));
  EXPECT_EQ("Not Found", HTTPResponseCodeAsString(HTTPResponseCode(404)));
  EXPECT_EQ("Unknown Code", HTTPResponseCodeAsString(HTTPResponseCode(999)));
  EXPECT_EQ("<UNINITIALIZED>", HTTPResponseCodeAsString(HTTPResponseCode(-1)));
}

TEST(HTTPMimeTypeTest, SmokeTest) {
  EXPECT_EQ("text/plain", GetFileMimeType("file.foo"));
  EXPECT_EQ("text/html", GetFileMimeType("file.html"));
  EXPECT_EQ("image/png", GetFileMimeType("file.png"));
  EXPECT_EQ("text/html", GetFileMimeType("dir.png/file.html"));
  EXPECT_EQ("text/plain", GetFileMimeType("dir.html/"));
  EXPECT_EQ("text/plain", GetFileMimeType("file.FOO"));
  EXPECT_EQ("text/html", GetFileMimeType("file.hTmL"));
  EXPECT_EQ("image/png", GetFileMimeType("file.PNG"));
}

// TODO(dkorolev): Figure out a way to test ConnectionResetByPeer exceptions.

#if 0

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
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Content-Length: 1000000\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("This body message terminates prematurely.\r\n", false);
  EXPECT_TRUE(connection_reset_by_peer);
  t.join();
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
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("10000\r\n, true");
  connection.BlockingWrite("This body message terminates prematurely.\r\n", false);
  EXPECT_TRUE(connection_reset_by_peer);
  t.join();
}

#endif
