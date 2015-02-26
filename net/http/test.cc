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
#include <atomic>

#include "../../port.h"

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
using bricks::net::HTTPResponseCodeValue;
using bricks::net::HTTPResponseCodeAsString;
using bricks::net::GetFileMimeType;
using bricks::net::DefaultInternalServerErrorMessage;
using bricks::net::HTTPNoBodyProvidedException;
using bricks::net::ConnectionResetByPeer;
using bricks::net::AttemptedToSendHTTPResponseMoreThanOnce;

static void ExpectToReceive(const std::string& golden, Connection& connection) {
  std::vector<char> response(golden.length());
  ASSERT_EQ(golden.length(),
            connection.BlockingRead(&response[0], golden.length(), Connection::FillFullBuffer));
  EXPECT_EQ(golden, std::string(response.begin(), response.end()));
}

struct HTTPTestObject {
  int number;
  std::string text;
  std::vector<int> array;  // Visual C++ does not support the `= { 1, 2, 3 };` non-static member initialization.
  HTTPTestObject() : number(42), text("text"), array({1, 2, 3}) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(number), CEREAL_NVP(text), CEREAL_NVP(array));
  }
};

TEST(PosixHTTPServerTest, Smoke) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/", c.HTTPRequest().RawPath());
               c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
             }

             // The `test_done` magic is required since the top-level HTTP-listening socket that accepts
             // connections
             // should not be closed until all the clients have finished reading their data.
             // This issue does not appear in `net/api` since the serving threads per port run forever,
             // however, extra logic is required to have this `net/http` test pass safely.
             // TODO(dkorolev): Use `WaitableAtomic` here.
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 4\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("BODY");
  connection.BlockingWrite("\r\n");
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 10\r\n"
      "\r\n"
      "Data: BODY",
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithArray) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("GET", c.HTTPRequest().Method());
               EXPECT_EQ("/aloha", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(std::vector<char>({'A', 'l', 'o', 'h', 'a'}));
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /aloha HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("\r\n");
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 5\r\n"
      "\r\n"
      "Aloha",
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithObject) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("GET", c.HTTPRequest().Method());
               EXPECT_EQ("/mahalo", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(HTTPTestObject());
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /mahalo HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("\r\n");
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 55\r\n"
      "\r\n"
      "{\"value0\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n",
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithNamedObject) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("GET", c.HTTPRequest().Method());
               EXPECT_EQ("/mahalo", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(HTTPTestObject(), "epic_object");
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /mahalo HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 60\r\n"
      "\r\n"
      "{\"epic_object\":{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}}\n",
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, SmokeChunkedResponse) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("GET", c.HTTPRequest().Method());
               EXPECT_EQ("/chunked", c.HTTPRequest().RawPath());
               auto r = c.SendChunkedHTTPResponse();
               r.Send("onetwothree");
               r.Send(std::vector<char>({'f', 'o', 'o'}));
               r.Send(HTTPTestObject());
               r.Send(HTTPTestObject(), "epic_chunk");
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /chunked HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("\r\n");
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: keep-alive\r\n"
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
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithHeaders) {
  std::atomic_bool test_done(false);
  thread
  t([&test_done](Socket s) {
      {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/header", c.HTTPRequest().RawPath());
        c.SendHTTPResponse("OK", HTTPResponseCode.OK, c.HTTPRequest().Body(), {{"foo", "bar"}, {"baz", "meh"}});
      }
      while (!test_done) {
        ;  // Spin lock.
      }
    },
    Socket(FLAGS_net_http_test_port));
  Connection connection(ClientSocket("localhost", FLAGS_net_http_test_port));
  connection.BlockingWrite("GET /header HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 19\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("custom_content_type");
  connection.BlockingWrite("\r\n");
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
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, LargeBody) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(std::string("Data: ") + c.HTTPRequest().Body());
             }
             while (!test_done) {
               ;  // Spin lock.
             }
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
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 1000006\r\n"
      "\r\n"
      "Data: " +
          body,
      connection);
  test_done = true;
  t.join();
}

TEST(PosixHTTPServerTest, ChunkedLargeBodyManyChunks) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(c.HTTPRequest().Body());
             }
             while (!test_done) {
               ;  // Spin lock.
             }
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
  ExpectToReceive(strings::Printf(
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n"
                      "%s",
                      static_cast<int>(body.length()),
                      body.c_str()),
                  connection);
  test_done = true;
  t.join();
}

// A dedicated test to cover buffer resize after the size of the next chunk has been received.
TEST(PosixHTTPServerTest, ChunkedBodyLargeFirstChunk) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/", c.HTTPRequest().RawPath());
               c.SendHTTPResponse(c.HTTPRequest().Body());
             }
             while (!test_done) {
               ;  // Spin lock.
             }
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
  ExpectToReceive(strings::Printf(
                      "HTTP/1.1 200 OK\r\n"
                      "Content-Type: text/plain\r\n"
                      "Connection: close\r\n"
                      "Content-Length: %d\r\n"
                      "\r\n"
                      "%s",
                      static_cast<int>(body.length()),
                      body.c_str()),
                  connection);
  test_done = true;
  t.join();
}

#ifndef BRICKS_WINDOWS
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

  static string Fetch(thread& server_thread,
                      std::atomic_bool& test_done,
                      const string& url,
                      const string& method) {
    const string result = Syscall(
        strings::Printf("curl -s -X %s localhost:%d%s", method.c_str(), FLAGS_net_http_test_port, url.c_str()));
    test_done = true;
    server_thread.join();
    return result;
  }

  static string FetchWithBody(thread& server_thread,
                              std::atomic_bool& test_done,
                              const string& url,
                              const string& method,
                              const string& data) {
    const string result = Syscall(strings::Printf("curl -s -X %s -d '%s' localhost:%d%s",
                                                  method.c_str(),
                                                  data.c_str(),
                                                  FLAGS_net_http_test_port,
                                                  url.c_str()));
    test_done = true;
    server_thread.join();
    return result;
  }
};
#endif

class HTTPClientImplPOSIX {
 public:
  static string Fetch(thread& server_thread,
                      std::atomic_bool& test_done,
                      const string& url,
                      const string& method) {
    return Impl(server_thread, test_done, url, method);
  }

  static string FetchWithBody(thread& server_thread,
                              std::atomic_bool& test_done,
                              const string& url,
                              const string& method,
                              const string& data) {
    return Impl(server_thread, test_done, url, method, true, data);
  }

 private:
  static string Impl(thread& server_thread,
                     std::atomic_bool& test_done,
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
    HTTPRequestData http_request(connection);
    assert(http_request.HasBody());
    const string body = http_request.Body();
    test_done = true;
    server_thread.join();
    return body;
  }
};

template <typename T>
class HTTPTest : public ::testing::Test {};

#ifndef BRICKS_WINDOWS
typedef ::testing::Types<HTTPClientImplPOSIX, HTTPClientImplCURL> HTTPClientImplsTypeList;
#else
typedef ::testing::Types<HTTPClientImplPOSIX> HTTPClientImplsTypeList;
#endif
TYPED_TEST_CASE(HTTPTest, HTTPClientImplsTypeList);

TYPED_TEST(HTTPTest, GET) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("GET", c.HTTPRequest().Method());
               EXPECT_EQ("/unittest?foo=bar", c.HTTPRequest().RawPath());
               EXPECT_EQ("/unittest", c.HTTPRequest().URL().path);
               EXPECT_EQ("bar", c.HTTPRequest().URL().query["foo"]);
               c.SendHTTPResponse("PASSED");
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("PASSED", TypeParam::Fetch(t, test_done, "/unittest?foo=bar", "GET"));
}

TYPED_TEST(HTTPTest, POST) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/unittest_post", c.HTTPRequest().RawPath());
               ASSERT_TRUE(c.HTTPRequest().HasBody()) << "WTF!";
               EXPECT_EQ("BAZINGA", c.HTTPRequest().Body());
               c.SendHTTPResponse("POSTED");
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("POSTED", TypeParam::FetchWithBody(t, test_done, "/unittest_post", "POST", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               EXPECT_EQ("POST", c.HTTPRequest().Method());
               EXPECT_EQ("/unittest_empty_post", c.HTTPRequest().RawPath());
               EXPECT_FALSE(c.HTTPRequest().HasBody());
               ASSERT_THROW(c.HTTPRequest().Body(), HTTPNoBodyProvidedException);
               c.SendHTTPResponse("ALMOST_POSTED");
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("ALMOST_POSTED", TypeParam::Fetch(t, test_done, "/unittest_empty_post", "POST"));
}

TYPED_TEST(HTTPTest, AttemptsToSendResponseTwice) {
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             {
               HTTPServerConnection c(s.Accept());
               c.SendHTTPResponse("one");
               ASSERT_THROW(c.SendHTTPResponse("two"), AttemptedToSendHTTPResponseMoreThanOnce);
               ASSERT_THROW(c.SendChunkedHTTPResponse().Send("three"), AttemptedToSendHTTPResponseMoreThanOnce);
             }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ("one", TypeParam::Fetch(t, test_done, "/", "GET"));
}

TYPED_TEST(HTTPTest, DoesNotSendResponseAtAll) {
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", DefaultInternalServerErrorMessage());
  std::atomic_bool test_done(false);
  thread t([&test_done](Socket s) {
             { HTTPServerConnection c(s.Accept()); }
             while (!test_done) {
               ;  // Spin lock.
             }
           },
           Socket(FLAGS_net_http_test_port));
  EXPECT_EQ(DefaultInternalServerErrorMessage(), TypeParam::Fetch(t, test_done, "/", "GET"));
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
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Content-Length: 1000000\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("This body message terminates prematurely.\r\n");
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
  connection.BlockingWrite("POST / HTTP/1.1\r\n");
  connection.BlockingWrite("Host: localhost\r\n");
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n");
  connection.BlockingWrite("\r\n");
  connection.BlockingWrite("10000\r\n");
  connection.BlockingWrite("This body message terminates prematurely.\r\n");
  EXPECT_TRUE(connection_reset_by_peer);
  t.join();
}

#endif
