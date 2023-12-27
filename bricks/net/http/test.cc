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

#define CURRENT_BRICKS_DEBUG_HTTP
#include "http.h"

#include "../../dflags/dflags.h"
#include "../../strings/printf.h"
#include "../../system/syscalls.h"

#include "../../../3rdparty/gtest/gtest-main-with-dflags.h"

using current::net::AttemptedToSendHTTPResponseMoreThanOnce;
using current::net::ChunkSizeNotAValidHEXValue;
using current::net::ClientSocket;
using current::net::Connection;
using current::net::ConnectionResetByPeer;
using current::net::DefaultInternalServerErrorMessage;
using current::net::GetFileMimeType;
using current::net::HTTPRequestData;
using current::net::HTTPResponseCodeAsString;
using current::net::HTTPResponseCodeValue;
using current::net::HTTPServerConnection;
using current::net::Socket;
using current::net::SocketException;

static void ExpectToReceive(const std::string& golden, Connection& connection) {
  std::vector<char> response(golden.length());
  try {
    ASSERT_EQ(golden.length(), connection.BlockingRead(&response[0], golden.length(), Connection::FillFullBuffer));
    EXPECT_EQ(golden, std::string(response.begin(), response.end()));
  } catch (const SocketException& e) {              // LCOV_EXCL_LINE
    ASSERT_TRUE(false) << e.DetailedDescription();  // LCOV_EXCL_LINE
  }
}

CURRENT_STRUCT(HTTPTestObject) {
  CURRENT_FIELD(number, int);
  CURRENT_FIELD(text, std::string);
  CURRENT_FIELD(array, std::vector<int>);
  CURRENT_DEFAULT_CONSTRUCTOR(HTTPTestObject) : number(42), text("text"), array({1, 2, 3}) {}
};

TEST(PosixHTTPServerTest, Smoke) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [port](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
        EXPECT_EQ(port, c.LocalIPAndPort().port);
        EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
        EXPECT_LT(0, c.RemoteIPAndPort().port);
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  EXPECT_EQ("127.0.0.1", connection.LocalIPAndPort().ip);
  EXPECT_LT(0, connection.LocalIPAndPort().port);
  EXPECT_EQ("127.0.0.1", connection.RemoteIPAndPort().ip);
  EXPECT_EQ(port, connection.RemoteIPAndPort().port);
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

TEST(PosixHTTPServerTest, SmokeWithTrailingSpaces) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [port](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
        EXPECT_EQ(port, c.LocalIPAndPort().port);
        EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
        EXPECT_LT(0, c.RemoteIPAndPort().port);
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  EXPECT_EQ("127.0.0.1", connection.LocalIPAndPort().ip);
  EXPECT_LT(0, connection.LocalIPAndPort().port);
  EXPECT_EQ("127.0.0.1", connection.RemoteIPAndPort().ip);
  EXPECT_EQ(port, connection.RemoteIPAndPort().port);
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host:\t\t  localhost  \r\n", true);
  connection.BlockingWrite("Content-Length:4  \t \t \r\n", true);
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/vector_char", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(std::vector<char>({'S', 't', 'r', 'i', 'n', 'g'}));
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/test_object", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(HTTPTestObject());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("GET /test_object HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Connection: close\r\n"
      "Content-Length: 44\r\n"
      "\r\n"
      "{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithObjectWithCORSHeader) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/test_object", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(HTTPTestObject(), HTTPResponseCode.OK, current::net::http::Headers().SetCORSHeader());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
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

TEST(PosixHTTPServerTest, SmokeChunkedResponse) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/chunked", c.HTTPRequest().RawPath());
        auto r = c.SendChunkedHTTPResponse();
        r.Send("onetwothree");
        r.Send(std::vector<char>({'f', 'o', 'o'}));
        r.Send(HTTPTestObject());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("GET /chunked HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: application/json; charset=utf-8\r\n"
      "Connection: keep-alive\r\n"
      "Transfer-Encoding: chunked\r\n"
      "\r\n"
      "B\r\n"
      "onetwothree\r\n"
      "3\r\n"
      "foo\r\n"
      "2C\r\n"
      "{\"number\":42,\"text\":\"text\",\"array\":[1,2,3]}\n\r\n"
      "0\r\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeChunkedResponseWithCORSHeader) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/chunked", c.HTTPRequest().RawPath());
        auto r = c.SendChunkedHTTPResponse(HTTPResponseCode.OK, current::net::http::Headers().SetCORSHeader());
        r.Send("onetwothree");
        r.Send(std::vector<char>({'f', 'o', 'o'}));
        r.Send(HTTPTestObject());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
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
      "0\r\n",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithHeaders) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/header", c.HTTPRequest().RawPath());
        c.SendHTTPResponse("OK",
                           HTTPResponseCode.OK,
                           current::net::http::Headers({{"foo", "bar"}, {"baz", "meh"}}),
                           "here_is_" + c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST /header HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Content-Length: 19\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("custom_content_type", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: here_is_custom_content_type\r\n"
      "Connection: close\r\n"
      "foo: bar\r\n"
      "baz: meh\r\n"
      "Content-Length: 2\r\n"
      "\r\n"
      "OK",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithLowercaseContentLength) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [port](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("127.0.0.1", c.LocalIPAndPort().ip);
        EXPECT_EQ(port, c.LocalIPAndPort().port);
        EXPECT_EQ("127.0.0.1", c.RemoteIPAndPort().ip);
        EXPECT_LT(0, c.RemoteIPAndPort().port);
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse("Data: " + c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  EXPECT_EQ("127.0.0.1", connection.LocalIPAndPort().ip);
  EXPECT_LT(0, connection.LocalIPAndPort().port);
  EXPECT_EQ("127.0.0.1", connection.RemoteIPAndPort().ip);
  EXPECT_EQ(port, connection.RemoteIPAndPort().port);
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("content_length: 2\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("Ok", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 8\r\n"
      "\r\n"
      "Data: Ok",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithMethodInHeader) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("PATCH", c.HTTPRequest().Method());
        EXPECT_EQ("/ugly", c.HTTPRequest().RawPath());
        EXPECT_EQ("YES", c.HTTPRequest().Body());
        c.SendHTTPResponse("PASS");
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST /ugly HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("X-HTTP-Method-Override: PATCH\r\n", true);
  connection.BlockingWrite("Content-Length: 3\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("YES\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 4\r\n"
      "\r\n"
      "PASS",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeWithLowercaseMethodInLowercaseHeader) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("PATCH", c.HTTPRequest().Method());
        EXPECT_EQ("/ugly2", c.HTTPRequest().RawPath());
        EXPECT_EQ("yes", c.HTTPRequest().Body());
        c.SendHTTPResponse("Pass");
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST /ugly2 HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("x-http_method_override: pAtCh\r\n", true);
  connection.BlockingWrite("Content-Length: 3\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("yes\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/plain\r\n"
      "Connection: close\r\n"
      "Content-Length: 4\r\n"
      "\r\n"
      "Pass",
      connection);
  t.join();
}

TEST(PosixHTTPServerTest, SmokeNoBodyForPOST) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::atomic_bool thrown(false);
  std::thread t(
      [&thrown](Socket s) {
        try {
          HTTPServerConnection c(s.Accept());
          ASSERT_TRUE(false);
        } catch (const current::net::HTTPRequestBodyLengthNotProvided&) {
          thrown = true;
        }
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("\r\n", true);
  connection.BlockingWrite("\r\n", false);
  ExpectToReceive(
      "HTTP/1.1 411 Length Required\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Connection: close\r\n"
      "Content-Length: 25\r\n"
      "\r\n"
      "<h1>LENGTH REQUIRED</h1>\n",
      connection);
  t.join();
  EXPECT_TRUE(thrown);
}

TEST(PosixHTTPServerTest, SmokePOSTBodyTooLong) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::atomic_bool thrown(false);
  std::thread t(
      [&thrown](Socket s) {
        try {
          HTTPServerConnection c(s.Accept());
          ASSERT_TRUE(false);
        } catch (const current::net::HTTPPayloadTooLarge&) {
          thrown = true;
        }
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  try {
    connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
    connection.BlockingWrite("Host: localhost\r\n", true);
    connection.BlockingWrite("Content-Length: 987654321000\r\n", true);
    connection.BlockingWrite("\r\n", true);
    connection.BlockingWrite("\r\n", true);
    // Intentionally send large enough payload to trigger race condition between
    // server closing the socket and client still sending the data.
    char dummy_buffer[1024 * 1024];
    connection.BlockingWrite(dummy_buffer, sizeof(dummy_buffer), false);
  } catch (current::net::SocketWriteException&) {
    // When server gets the `Content-Length` that exceeds the supported maximum
    // it closes the connection (see https://tools.ietf.org/html/rfc7231#section-6.5.11).
    // Thus, client may face a situation when it still tries to write to the socket
    // already closed by server, so it needs to properly handle this expected behavior.
  }
  ExpectToReceive(
      "HTTP/1.1 413 Request Entity Too Large\r\n"
      "Content-Type: text/html; charset=utf-8\r\n"
      "Connection: close\r\n"
      "Content-Length: 26\r\n"
      "\r\n"
      "<h1>ENTITY TOO LARGE</h1>\n",
      connection);
  t.join();
  EXPECT_TRUE(thrown);
}

TEST(PosixHTTPServerTest, LargeBody) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(std::string("Data: ") + c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  std::string body(1000000, '.');
  for (size_t i = 0; i < body.length(); ++i) {
    body[i] = 'A' + (i % 26);
  }
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite(current::strings::Printf("Content-Length: %d\r\n", static_cast<int>(body.length())), true);
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);
  std::string chunk(10, '.');
  std::string body = "";
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
  ExpectToReceive(current::strings::Printf("HTTP/1.1 200 OK\r\n"
                                           "Content-Type: text/plain\r\n"
                                           "Connection: close\r\n"
                                           "Content-Length: %d\r\n"
                                           "\r\n",
                                           static_cast<int>(body.length())) +
                      body,
                  connection);
  t.join();
}

TEST(PosixHTTPServerTest, ChunkedSmoke) {
  const auto EchoServerThreadEntry = [](Socket s) {
    HTTPServerConnection c(s.Accept());
    EXPECT_EQ("POST", c.HTTPRequest().Method());
    EXPECT_EQ("/", c.HTTPRequest().RawPath());
    c.SendHTTPResponse(c.HTTPRequest().Body());
  };
  std::string body;
  std::string chunk(10, '.');
  for (int i = 0; i < 10000; ++i) {
    for (int j = 0; j < 10; ++j) {
      chunk[j] = static_cast<char>('A' + ((i + j) % 26));
    }
    body += chunk;
  }
  for (uint64_t length = body.length(); length > 1500; length -= (length / 7)) {
    const auto offset = body.length() - length;
    for (uint64_t chunks = length; chunks; chunks = chunks * 2 / 3) {
      std::string chunked_body;
      for (uint64_t i = 0; i < chunks; ++i) {
        const uint64_t start = offset + length * i / chunks;
        const uint64_t end = offset + length * (i + 1) / chunks;
        chunked_body += current::strings::Printf("%llX\r\n", static_cast<long long>(end - start)) +
                        body.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
      }

      auto local_reserved_port = current::net::ReserveLocalPort();
      const int local_port = local_reserved_port;
      std::thread t(EchoServerThreadEntry, std::move(local_reserved_port));
      Connection connection(ClientSocket("localhost", local_port));
      connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
      connection.BlockingWrite("Host: localhost\r\n", true);
      connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
      connection.BlockingWrite("\r\n", true);
      connection.BlockingWrite(chunked_body, true);
      connection.BlockingWrite("0\r\n", false);
      ExpectToReceive(current::strings::Printf("HTTP/1.1 200 OK\r\n"
                                               "Content-Type: text/plain\r\n"
                                               "Connection: close\r\n"
                                               "Content-Length: %d\r\n"
                                               "\r\n",
                                               static_cast<int>(length)) +
                          body.substr(static_cast<size_t>(offset)),
                      connection);
      t.join();
    }
  }
}

// `CURRENT_HTTP_DATA_JOURNAL_ENABLED` will be `#define`-d if `CURRENT_BRICKS_DEBUG_HTTP`
// was `#define`-d _the first time_ `impl/server.h` was included.
// This is to guard against header inclusion order, particularly in the top-level `make test`.
#ifdef CURRENT_HTTP_DATA_JOURNAL_ENABLED
TEST(PosixHTTPServerTest, ChunkedBoundaryCases) {
  const auto EchoServerThreadEntryWithOptions =
      [](Socket s, const int initial_buffer_size, const double buffer_growth_k) {
        HTTPServerConnection c(
            s.Accept(), current::net::HTTPDefaultHelper::ConstructionParams(), initial_buffer_size, buffer_growth_k);
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(c.HTTPRequest().Body());
      };
  const std::string headers = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked";
  const std::string headers_paddings[] = {std::string(100 - headers.length() - 4 /*\r\n\r\n*/, ' '),
                                          std::string(100 - headers.length() - 5 /*\r\n\r\n\0*/, ' '),
                                          std::string(100 - headers.length() - 6, ' ')};
  // Body for all the test cases: four chunks of 90, 399, 393 and 792 bytes.
  const std::string body = std::string(90, 'a') + std::string(399, 'b') + std::string(393, 'c') + std::string(792, 'd');
  // Encoded lenghts of the chunks: 94, 404, 398 and 797.
  const std::string chunks[] = {"5A\r\n" + std::string(90, 'a'),
                                "18F\r\n" + std::string(399, 'b'),
                                "189\r\n" + std::string(393, 'c'),
                                "318\r\n" + std::string(792, 'd')};
  const std::string expect_to_receive = current::strings::Printf(
                                            "HTTP/1.1 200 OK\r\n"
                                            "Content-Type: text/plain\r\n"
                                            "Connection: close\r\n"
                                            "Content-Length: %d\r\n"
                                            "\r\n",
                                            static_cast<int>(body.length())) +
                                        body;

  // Test cases block 1: http header and all the chunks are sent instantly.
  const std::vector<std::string> expected_events_block1[] = {
      // clang-format off
    {// Test case 1.0 (100 bytes header).
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "read 100 bytes while requested 100 (buffer offset 99)\n",
      "resize the buffer 200 -> 400\n",
      "http header is parsed\n",
      "process a 90 bytes long chunk\n",
      "memmove 0 bytes from offset 199 to fit the entire chunk\n",
      "read 399 more bytes of a chunk at offset 0\n",
      "process a 399 bytes long chunk\n",
      "read 399 bytes while requested 399 (buffer offset 0)\n",
      "resize the buffer 400 -> 800\n",
      "process a 393 bytes long chunk\n",
      "memmove 1 bytes from offset 398 to the beginning\n",
      "read 798 bytes while requested 798 (buffer offset 1)\n",
      "resize the buffer 800 -> 1600\n",
      "process a 792 bytes long chunk\n",
      "memmove 2 bytes from offset 797 to the beginning\n",
      "read 1 bytes while requested 1597 (buffer offset 2)\n"
    },
    {// Test case 1.1 (99 bytes header).
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "http header is parsed\n",
      "read 199 bytes while requested 199 (buffer offset 0)\n",
      "resize the buffer 200 -> 400\n",
      "process a 90 bytes long chunk\n",
      "memmove 100 bytes from offset 99 to fit the entire chunk\n",
      "read 299 more bytes of a chunk at offset 100\n",
      "process a 399 bytes long chunk\n",
      "read 399 bytes while requested 399 (buffer offset 0)\n",
      "resize the buffer 400 -> 800\n",
      "process a 393 bytes long chunk\n",
      "memmove 1 bytes from offset 398 to the beginning\n",
      "read 798 bytes while requested 798 (buffer offset 1)\n",
      "resize the buffer 800 -> 1600\n",
      "process a 792 bytes long chunk\n",
      "memmove 2 bytes from offset 797 to the beginning\n",
      "read 1 bytes while requested 1597 (buffer offset 2)\n"
    },
    {// Test case 1.2 (98 bytes header).
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "http header is parsed\n",
      "memmove 1 bytes from offset 98 to the beginning\n",
      "read 198 bytes while requested 198 (buffer offset 1)\n",
      "resize the buffer 200 -> 400\n",
      "process a 90 bytes long chunk\n",
      "memmove 100 bytes from offset 99 to fit the entire chunk\n",
      "read 299 more bytes of a chunk at offset 100\n",
      "process a 399 bytes long chunk\n",
      "read 399 bytes while requested 399 (buffer offset 0)\n",
      "resize the buffer 400 -> 800\n",
      "process a 393 bytes long chunk\n",
      "memmove 1 bytes from offset 398 to the beginning\n",
      "read 798 bytes while requested 798 (buffer offset 1)\n",
      "resize the buffer 800 -> 1600\n",
      "process a 792 bytes long chunk\n",
      "memmove 2 bytes from offset 797 to the beginning\n",
      "read 1 bytes while requested 1597 (buffer offset 2)\n"
    }
      // clang-format on
  };

  const auto chunked_body = chunks[0] + chunks[1] + chunks[2] + chunks[3] + "0\r\n";
  for (size_t i = 0; i < 3; ++i) {
    current::net::HTTPDataJournal().Start();
    auto local_reserved_port = current::net::ReserveLocalPort();
    const int local_port = local_reserved_port;
    std::thread t(EchoServerThreadEntryWithOptions, std::move(local_reserved_port), 100, 2.0);
    Connection connection(ClientSocket("localhost", local_port));
    connection.BlockingWrite(headers + headers_paddings[i] + "\r\n\r\n" + chunked_body, false);
    ExpectToReceive(expect_to_receive, connection);
    t.join();
    EXPECT_EQ(expected_events_block1[i].size(), current::net::HTTPDataJournal().events.size());
    for (size_t j = 0, sz = std::min(expected_events_block1[i].size(), current::net::HTTPDataJournal().events.size());
         j < sz;
         ++j) {
      EXPECT_EQ(expected_events_block1[i][j], current::net::HTTPDataJournal().events[j]);
    }
    current::net::HTTPDataJournal().Stop();
  }

  // Test cases block 2: http header and each chunk is sent separatedly with sufficiently long delays in between.
  const std::vector<std::string> expected_events_block2[] = {
      // clang-format off
    {// Test case 2.0 (100 bytes header).
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "read 1 bytes while requested 100 (buffer offset 99)\n",
      "http header is parsed\n",
      "read 94 bytes while requested 199 (buffer offset 0)\n",
      "process a 90 bytes long chunk\n",
      "read 199 bytes while requested 199 (buffer offset 0)\n",
      "resize the buffer 200 -> 400\n",
      "memmove 194 bytes from offset 5 to fit the entire chunk\n",
      "read 205 more bytes of a chunk at offset 194\n",
      "process a 399 bytes long chunk\n",
      "read 398 bytes while requested 399 (buffer offset 0)\n",
      "process a 393 bytes long chunk\n",
      "read 399 bytes while requested 399 (buffer offset 0)\n",
      "resize the buffer 400 -> 800\n",
      "read 398 more bytes of a chunk at offset 399\n",
      "process a 792 bytes long chunk\n",
      "read 3 bytes while requested 799 (buffer offset 0)\n"
    },
    {// Test case 2.1 (99 bytes header).
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "http header is parsed\n",
      "read 94 bytes while requested 199 (buffer offset 0)\n",
      "process a 90 bytes long chunk\n",
      "read 199 bytes while requested 199 (buffer offset 0)\n",
      "resize the buffer 200 -> 400\n",
      "memmove 194 bytes from offset 5 to fit the entire chunk\n",
      "read 205 more bytes of a chunk at offset 194\n",
      "process a 399 bytes long chunk\n",
      "read 398 bytes while requested 399 (buffer offset 0)\n",
      "process a 393 bytes long chunk\n",
      "read 399 bytes while requested 399 (buffer offset 0)\n",
      "resize the buffer 400 -> 800\n",
      "read 398 more bytes of a chunk at offset 399\n",
      "process a 792 bytes long chunk\n",
      "read 3 bytes while requested 799 (buffer offset 0)\n"
    },
    {// Test case 2.2 (98 bytes header).
      "read 98 bytes while requested 99 (buffer offset 0)\n",
      "http header is parsed\n",
      "read 94 bytes while requested 99 (buffer offset 0)\n",
      "process a 90 bytes long chunk\n",
      "read 99 bytes while requested 99 (buffer offset 0)\n",
      "resize the buffer 100 -> 200\n",
      "resize the buffer 200 -> 405 to fit the entire chunk\n",
      "read 305 more bytes of a chunk at offset 99\n",
      "process a 399 bytes long chunk\n",
      "read 398 bytes while requested 404 (buffer offset 0)\n",
      "process a 393 bytes long chunk\n",
      "read 404 bytes while requested 404 (buffer offset 0)\n",
      "resize the buffer 405 -> 810\n",
      "read 393 more bytes of a chunk at offset 404\n",
      "process a 792 bytes long chunk\n",
      "read 3 bytes while requested 809 (buffer offset 0)\n"
    }
      // clang-format on
  };

  for (size_t i = 0; i < 3; ++i) {
    current::net::HTTPDataJournal().Start();
    auto local_reserved_port = current::net::ReserveLocalPort();
    const int local_port = local_reserved_port;
    std::thread t(EchoServerThreadEntryWithOptions, std::move(local_reserved_port), 100, 2.0);
    Connection connection(ClientSocket("localhost", local_port));
    connection.BlockingWrite(headers + headers_paddings[i] + "\r\n\r\n", false);
    for (const auto& chunk : chunks) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      connection.BlockingWrite(chunk, false);
    }
    connection.BlockingWrite("0\r\n", false);
    ExpectToReceive(expect_to_receive, connection);
    t.join();
    EXPECT_EQ(expected_events_block2[i].size(), current::net::HTTPDataJournal().events.size());
    for (size_t j = 0, sz = std::min(expected_events_block2[i].size(), current::net::HTTPDataJournal().events.size());
         j < sz;
         ++j) {
      EXPECT_EQ(expected_events_block2[i][j], current::net::HTTPDataJournal().events[j]);
    }
    current::net::HTTPDataJournal().Stop();
  }
}
#endif  // CURRENT_HTTP_DATA_JOURNAL_ENABLED

TEST(PosixHTTPServerTest, InvalidHEXAsChunkSizeDoesNotKillServer) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::atomic_bool wrong_chunk_size_exception_thrown(false);
  std::thread t(
      [&wrong_chunk_size_exception_thrown](Socket s) {
        try {
          HTTPServerConnection c(s.Accept());
        } catch (const ChunkSizeNotAValidHEXValue&) {
          wrong_chunk_size_exception_thrown = true;
        }
      },
      std::move(reserved_port));

  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);

  // Chunk size should be hexadecimal, but `GG` sure is not.
  connection.BlockingWrite("GG\r\n", true);

  // This part is, in fact, redundant.
  connection.BlockingWrite("buffalo buffalo buffalo buffalo buffalo\r\n", true);
  connection.BlockingWrite("0\r\n", false);

  // Wait for the server thread to attempt to accept the connection and fail.
  t.join();

  // Confirm the server did throw an exception attempting to accept the connection.
  ASSERT_TRUE(wrong_chunk_size_exception_thrown);
}

// A dedicated test to cover buffer resize after the size of the next chunk has been received.
TEST(PosixHTTPServerTest, ChunkedBodyLargeFirstChunk) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/", c.HTTPRequest().RawPath());
        c.SendHTTPResponse(c.HTTPRequest().Body());
      },
      std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
  connection.BlockingWrite("POST / HTTP/1.1\r\n", true);
  connection.BlockingWrite("Host: localhost\r\n", true);
  connection.BlockingWrite("Transfer-Encoding: chunked\r\n", true);
  connection.BlockingWrite("\r\n", true);
  std::string chunk(10000, '.');
  std::string body = "";
  for (size_t i = 0; i < 10; ++i) {
    connection.BlockingWrite(current::strings::Printf("%X\r\n", 10000), true);
    for (size_t j = 0; j < 10000; ++j) {
      chunk[j] = 'a' + ((i + j) % 26);
    }
    connection.BlockingWrite(chunk, true);
    body += chunk;
  }
  connection.BlockingWrite("0\r\n", false);
  ExpectToReceive(current::strings::Printf("HTTP/1.1 200 OK\r\n"
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
  static std::string MakeGetRequest(std::thread& server_thread, int port, const std::string& url) {
    const auto cmd = current::strings::Printf("curl -s localhost:%d%s", port, url.c_str());
    const std::string result = current::bricks::system::SystemCallReadPipe(cmd).ReadLine();
    server_thread.join();
    return result;
  }

  static std::string MakePostRequest(std::thread& server_thread,
                                     int port,
                                     const std::string& url,
                                     const std::string& data) {
    const auto cmd = current::strings::Printf("curl -s -d '%s' localhost:%d%s", data.c_str(), port, url.c_str());
    const std::string result = current::bricks::system::SystemCallReadPipe(cmd).ReadLine();
    server_thread.join();
    return result;
  }
};
#endif

class HTTPClientImplPOSIX {
 public:
  static std::string MakeGetRequest(std::thread& server_thread, int port, const std::string& url) {
    return Impl(server_thread, port, url, "GET");
  }

  static std::string MakePostRequest(std::thread& server_thread,
                                     int port,
                                     const std::string& url,
                                     const std::string& data) {
    return Impl(server_thread, port, url, "POST", true, data);
  }

 private:
  static std::string Impl(std::thread& server_thread,
                          int port,
                          const std::string& url,
                          const std::string& method,
                          bool has_data = false,
                          const std::string& data = "") {
    Connection connection(ClientSocket("localhost", port));
    connection.BlockingWrite(method + ' ' + url + "\r\n", true);
    if (has_data) {
      connection.BlockingWrite("Content-Length: " + std::to_string(data.length()) + "\r\n", true);
      connection.BlockingWrite("\r\n", true);
      connection.BlockingWrite(data, false);
    } else {
      connection.BlockingWrite("\r\n", false);
    }
    HTTPRequestData http_request(connection);
    CURRENT_ASSERT(!http_request.Body().empty());
    const std::string body = http_request.Body();
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("GET", c.HTTPRequest().Method());
        EXPECT_EQ("/unittest?foo=bar", c.HTTPRequest().RawPath());
        EXPECT_EQ("/unittest", c.HTTPRequest().URL().path);
        EXPECT_EQ("bar", c.HTTPRequest().URL().query["foo"]);
        c.SendHTTPResponse("PASSED");
      },
      std::move(reserved_port));
  EXPECT_EQ("PASSED", TypeParam::MakeGetRequest(t, port, "/unittest?foo=bar"));
}

TYPED_TEST(HTTPTest, POST) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/unittest_post", c.HTTPRequest().RawPath());
        EXPECT_EQ(7u, c.HTTPRequest().BodyLength());
        EXPECT_EQ("BAZINGA", c.HTTPRequest().Body());
        c.SendHTTPResponse("POSTED");
      },
      std::move(reserved_port));
  EXPECT_EQ("POSTED", TypeParam::MakePostRequest(t, port, "/unittest_post", "BAZINGA"));
}

TYPED_TEST(HTTPTest, NoBodyPOST) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        EXPECT_EQ("POST", c.HTTPRequest().Method());
        EXPECT_EQ("/unittest_empty_post", c.HTTPRequest().RawPath());
        EXPECT_EQ(0u, c.HTTPRequest().BodyLength());
        c.SendHTTPResponse("ALMOST_POSTED");
      },
      std::move(reserved_port));
  EXPECT_EQ("ALMOST_POSTED", TypeParam::MakePostRequest(t, port, "/unittest_empty_post", ""));
}

TYPED_TEST(HTTPTest, AttemptsToSendResponseTwice) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  std::thread t(
      [](Socket s) {
        HTTPServerConnection c(s.Accept());
        c.SendHTTPResponse("one");
        ASSERT_THROW(c.SendHTTPResponse("two"), AttemptedToSendHTTPResponseMoreThanOnce);
        ASSERT_THROW(c.SendChunkedHTTPResponse().Send("three"), AttemptedToSendHTTPResponseMoreThanOnce);
      },
      std::move(reserved_port));
  EXPECT_EQ("one", TypeParam::MakeGetRequest(t, port, "/"));
}

TYPED_TEST(HTTPTest, DoesNotSendResponseAtAll) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  EXPECT_EQ("<h1>INTERNAL SERVER ERROR</h1>\n", DefaultInternalServerErrorMessage());
  std::thread t([](Socket s) { HTTPServerConnection c(s.Accept()); }, std::move(reserved_port));
  EXPECT_EQ(current::strings::Trim(DefaultInternalServerErrorMessage()),
            current::strings::Trim(TypeParam::MakeGetRequest(t, port, "/")));
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
  EXPECT_EQ("application/javascript", GetFileMimeType("file.js"));
  EXPECT_EQ("text/css", GetFileMimeType("file.css"));
  EXPECT_EQ("application/json; charset=utf-8", GetFileMimeType("file.json"));
  EXPECT_EQ("application/json; charset=utf-8", GetFileMimeType("file.js.map"));
  EXPECT_EQ("application/json; charset=utf-8", GetFileMimeType("file.css.map"));
  EXPECT_EQ("image/x-icon", GetFileMimeType("favicon.ico"));
}

// TODO(dkorolev): Figure out a way to test ConnectionResetByPeer exceptions.

#if 0

TEST(HTTPServerTest, ConnectionResetByPeerException) {
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  bool connection_reset_by_peer = false;
  std::thread t([&connection_reset_by_peer](Socket s) {
             try {
               HTTPServerConnection(s.Accept());
             } catch (const ConnectionResetByPeer&) {
               connection_reset_by_peer = true;
             }
           },
           std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
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
  auto reserved_port = current::net::ReserveLocalPort();
  const int port = reserved_port;
  bool connection_reset_by_peer = false;
  std::thread t([&connection_reset_by_peer](Socket s) {
             try {
               HTTPServerConnection(s.Accept());
             } catch (const ConnectionResetByPeer&) {
               connection_reset_by_peer = true;
             }
           },
           std::move(reserved_port));
  Connection connection(ClientSocket("localhost", port));
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
