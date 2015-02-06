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
#include "../../cerealize/cerealize.h"

DEFINE_int32(net_http_test_port, 8080, "Local port to use for the test HTTP server.");

using std::string;
using std::thread;
using std::to_string;

using namespace bricks;

using bricks::net::Socket;
using bricks::net::ClientSocket;
using bricks::net::Connection;
using bricks::net::HTTPServerConnection;
using bricks::net::HTTPRequestData;
using bricks::net::HTTPResponseCode;
using bricks::net::HTTPResponseCodeAsStringGenerator;
using bricks::net::HTTPNoBodyProvidedException;
using bricks::net::ConnectionResetByPeer;

struct HTTPTestObject {
  int number = 42;
  std::string text = "text";
  std::vector<int> array = {1, 2, 3};
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(number), CEREAL_NVP(text), CEREAL_NVP(array));
  }
};

TEST(PosixHTTPServerTest, Smoke) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/", c.HTTPRequest().RawPath());
             c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
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
      "Data: BODY",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, SmokeWithArray) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("GET", c.HTTPRequest().Method());
             EXPECT_EQ("/aloha", c.HTTPRequest().RawPath());
             c.SendHTTPResponse(std::vector<char>({'A', 'l', 'o', 'h', 'a'}));
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /aloha HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  // The last "\r\n" and EOF are unnecessary, but conventional here. See the test below w/o them.
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "Aloha",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, SmokeWithObject) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("GET", c.HTTPRequest().Method());
             EXPECT_EQ("/mahalo", c.HTTPRequest().RawPath());
             c.SendHTTPResponse(HTTPTestObject());
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /mahalo HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  // The last "\r\n" and EOF are unnecessary, but conventional here. See the test below w/o them.
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 55\r\n"
      "\r\n"
      "{\"value0\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, SmokeWithNamedObject) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("GET", c.HTTPRequest().Method());
             EXPECT_EQ("/mahalo", c.HTTPRequest().RawPath());
             c.SendHTTPResponse(HTTPTestObject(), "epic_object");
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /mahalo HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 60\r\n"
      "\r\n"
      "{\"epic_object\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n",
      connection.BlockingReadUntilEOF());
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
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /chunked HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "B\r\n"
      "onetwothree\r\n"
      "3\r\n"
      "foo\r\n"
      "37\r\n"
      "{\"value0\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n\r\n"
      "3B\r\n"
      "{\"epic_chunk\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n\r\n"
      "0\r\n",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, SmokeWithHeaders) {
  thread
  t([](Socket s) {
      HTTPServerConnection c(s.Accept());
      EXPECT_EQ("GET", c.HTTPRequest().Method());
      EXPECT_EQ("/header", c.HTTPRequest().RawPath());
      c.SendHTTPResponse("OK", HTTPResponseCode::OK, c.HTTPRequest().Body(), {{"foo", "bar"}, {"baz", "meh"}});
    },
    Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /header HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 19\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("custom_content_type");
  connection.BlockingWrite("\r\n");
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: custom_content_type\r\n"
      "foo: bar\r\n"
      "baz: meh\r\n"
      "Content-Length: 2\r\n"
      "\r\n"
      "OK",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, NoEOF) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/", c.HTTPRequest().RawPath());
             c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
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
      "Data: NOEOF",
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, LargeBody) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/", c.HTTPRequest().RawPath());
             c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
           },
           Socket(FLAGS_net_http_test_port));
  string body(1000000, '.');
  for (size_t i = 0; i < body.length(); ++i) {
    body[i] = 'A' + (i % 26);
  }
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite(strings::Printf("Content-Length: %d\r\n", static_cast<int>(body.length())));
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite(body);
  connection.SendEOF();
  t.join();
  EXPECT_EQ(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Content-Length: 1000006\r\n"
      "\r\n"
      "Data: " +
          body,
      connection.BlockingReadUntilEOF());
}

TEST(PosixHTTPServerTest, ChunkedLargeBodyManyChunks) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/", c.HTTPRequest().RawPath());
             c.SendHTTPResponse(c.HTTPRequest().Body());
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
                "%s",
                static_cast<int>(body.length()),
                body.c_str()),
            connection.BlockingReadUntilEOF());
}

// A dedicated test to cover buffer resize after the size of the next chunk has been received.
TEST(PosixHTTPServerTest, ChunkedBodyLargeFirstChunk) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/", c.HTTPRequest().RawPath());
             c.SendHTTPResponse(c.HTTPRequest().Body());
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
  }
  connection.BlockingWrite("0\r\n");
  t.join();
  EXPECT_EQ(strings::Printf(
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/plain\r\n"
                "Content-Length: %d\r\n"
                "\r\n"
                "%s",
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
    HTTPRequestData http_request(connection);
    assert(http_request.HasBody());
    const string body = http_request.Body();
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
             EXPECT_EQ("GET", c.HTTPRequest().Method());
             EXPECT_EQ("/unittest?foo=bar", c.HTTPRequest().RawPath());
             EXPECT_EQ("/unittest", c.HTTPRequest().URL().path);
             EXPECT_EQ("bar", c.HTTPRequest().URL().query["foo"]);
             c.SendHTTPResponse("PASSED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, "/unittest?foo=bar", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/unittest_post", c.HTTPRequest().RawPath());
             ASSERT_TRUE(c.HTTPRequest().HasBody()) << "WTF!";
             EXPECT_EQ("BAZINGA", c.HTTPRequest().Body());
             c.SendHTTPResponse("POSTED");
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  thread t([](Socket s) {
             HTTPServerConnection c(s.Accept());
             EXPECT_EQ("POST", c.HTTPRequest().Method());
             EXPECT_EQ("/unittest_empty_post", c.HTTPRequest().RawPath());
             EXPECT_FALSE(c.HTTPRequest().HasBody());
             ASSERT_THROW(c.HTTPRequest().Body(), HTTPNoBodyProvidedException);
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
