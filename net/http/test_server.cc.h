// HTTP server for the unit test in `net/api/test.cc`.
//
// This implementation uses pure HTTP connection, it does not use the bricks::net::api magic.
//
// This is a `.cc.h` file, since it violates the one-definition rule for gtest, and thus
// should not be an independent header. It is nonetheless #include-d by net/api/test.cc.

#ifndef BRICKS_NET_HTTP_TEST_SERVER_CC_H
#define BRICKS_NET_HTTP_TEST_SERVER_CC_H

#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "http.h"
#include "../url/url.h"

#include "../../3party/gtest/gtest.h"

static const uint64_t kDelayBetweenChunksInMilliseconds = 10;

namespace bricks {
namespace net {
namespace http {
namespace test {

class TestHTTPServer_HTTPImpl {
 public:
  class Impl {
   public:
    Impl(int port) : port_(port), thread_(&Impl::Code, Socket(port_)) {}

    ~Impl() {
      Connection(ClientSocket("localhost", port_))
          .BlockingWrite("GET /killtestserver HTTP/1.1\r\n\r\n")
          .BlockingReadUntilEOF();
      thread_.join();
    }

   private:
    static void Code(Socket socket) {
      bool terminating = false;
      while (!terminating) {
        HTTPServerConnection connection(socket.Accept());
        const auto& message = connection.Message();
        const std::string method = message.Method();
        const url::URL url = url::URL(message.Path());
        if (method == "GET") {
          if (url.path == "/killtestserver") {
            connection.SendHTTPResponse("Roger that.");
            terminating = true;
          } else if (url.path == "/drip") {
            const size_t numbytes = atoi(url.query["numbytes"].c_str());
            Connection& c = connection.RawConnection();
            // Send in some extra, RFC-allowed, whitespaces.
            c.BlockingWrite("HTTP/1.1  \t  \t\t  200\t    \t\t\t\t  OK\r\n");
            c.BlockingWrite("Transfer-Encoding: chunked\r\n");
            c.BlockingWrite("Content-Type: application/octet-stream\r\n");
            c.BlockingWrite("\r\n");
            std::this_thread::sleep_for(std::chrono::milliseconds(kDelayBetweenChunksInMilliseconds));
            for (size_t i = 0; i < numbytes; ++i) {
              c.BlockingWrite("1\r\n*\r\n");
              std::this_thread::sleep_for(std::chrono::milliseconds(kDelayBetweenChunksInMilliseconds));
            }
            c.BlockingWrite("0\r\n\r\n");  // Line ending as httpbin.org seems to do it. -- D.K.
            connection.RawConnection().SendEOF();
          } else if (url.path == "/status/403") {
            connection.SendHTTPResponse("", HTTPResponseCode::Forbidden);
          } else if (url.path == "/get") {
            connection.SendHTTPResponse("{\"Aloha\": \"" + url.query["Aloha"] + "\"}\n");
          } else if (url.path == "/user-agent") {
            // TODO(dkorolev): Add parsing User-Agent to Bricks' HTTP headers parser.
            // The response should be taken from the header, not hardcoded to be "Aloha User Agent".
            connection.SendHTTPResponse("Aloha User Agent");
          } else if (url.path == "/redirect-to") {
            HTTPHeadersType headers;
            headers.push_back(std::make_pair("Location", url.query["url"]));
            connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
          } else if (url.path == "/redirect-loop") {
            HTTPHeadersType headers;
            headers.push_back(std::make_pair("Location", "/redirect-loop-2"));
            connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
          } else if (url.path == "/redirect-loop-2") {
            HTTPHeadersType headers;
            headers.push_back(std::make_pair("Location", "/redirect-loop-3"));
            connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
          } else if (url.path == "/redirect-loop-3") {
            HTTPHeadersType headers;
            headers.push_back(std::make_pair("Location", "/redirect-loop"));
            connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
          } else {
            ASSERT_TRUE(false) << "GET not implemented for: " << message.Path();  // LCOV_EXCL_LINE
          }
        } else if (method == "POST") {
          if (url.path == "/post") {
            ASSERT_TRUE(message.HasBody());
            connection.SendHTTPResponse("{\"data\": \"" + message.Body() + "\"}\n");
          } else {
            ASSERT_TRUE(false) << "POST not implemented for: " << message.Path();  // LCOV_EXCL_LINE
          }
        } else {
          ASSERT_TRUE(false) << "Method not implemented: " << message.Method();  // LCOV_EXCL_LINE
        }
      }
    }
    const int port_;
    std::thread thread_;
  };

  class UniqueImpl {
   public:
    UniqueImpl(int port) : port_(port), impl_(new Impl(port)) {}

    std::string BaseURL() const { return std::string("http://localhost:") + std::to_string(port_); }

   private:
    const int port_;
    std::unique_ptr<Impl> impl_;
  };

  static UniqueImpl Spawn(int port) { return UniqueImpl(port); }
};

}  // namespace test
}  // namespace http
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_HTTP_TEST_SERVER_CC_H
