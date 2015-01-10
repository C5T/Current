// HTTP server for the unit test in `net/api/test.cc`.
//
// This implementation uses the API that Bricks provides.
// Compare this code to the one in `net/http/test_server.cc.h`.
//
// This is a `.cc.h` file, since it violates the one-definition rule for gtest, and thus
// should not be an independent header. It is nonetheless #include-d by net/api/test.cc.

#ifndef BRICKS_NET_API_TEST_SERVER_CC_H
#define BRICKS_NET_API_TEST_SERVER_CC_H

#include <thread>

#include "api.h"
#include "../url/url.h"
#include "../http/test_server.cc.h"

#include "../../strings/printf.h"

namespace bricks {
namespace net {
namespace api {
namespace test {

enum class Method { UNINITIALIZED, GET, POST };

struct Request {
  const url::URL& url;
  HTTPServerConnection& connection;
  const HTTPReceivedMessage& message;
};

struct Routes {
  struct Helper {
    struct Impl {
      explicit Impl(Routes* routes) : routes_(routes) {}
      ~Impl() {
        assert(method_ != Method::UNINITIALIZED);
        assert(!path_.empty());
        assert(handler_);
        routes_->AddRoute(method_, path_, handler_);
      }
      void SetHandler(std::function<void(Request&&)>&& f) {
        assert(!handler_);
        handler_ = f;
      }
      void AddConstraint(Method method) {
        assert(method != Method::UNINITIALIZED);
        assert(method_ == Method::UNINITIALIZED);
        method_ = method;
      }
      void AddConstraint(const std::string& path) {
        assert(!path.empty());
        assert(path_.empty());
        path_ = path;
      }

      Routes* routes_;
      std::function<void(Request&&)> handler_;

      Method method_ = Method::UNINITIALIZED;
      std::string path_ = "";

      Impl() = delete;
      Impl(const Impl&) = delete;
      Impl(Impl&&) = delete;
      void operator=(const Impl&) = delete;
      void operator=(Impl&&) = delete;
    };

    Helper(Routes* routes) : impl_(new Impl(routes)) {}
    Helper(Helper&& rhs) { impl_.swap(rhs.impl_); }
    void operator=(std::function<void(Request&&)>&& f) {
      assert(impl_);
      impl_->SetHandler(std::forward<std::function<void(Request && )>>(f));
    }
    template <typename T>
    Helper& operator[](T&& constraint) {
      assert(impl_);
      impl_->AddConstraint(std::forward<T>(constraint));
      return *this;
    }

    std::unique_ptr<Impl> impl_;
  };

  template <typename T>
  Helper operator[](T&& constraint) {
    Helper helper(this);
    helper[constraint];
    return helper;
  }

  void AddRoute(Method method, const std::string& path, std::function<void(Request&&)> handler) {
    routes_[std::make_pair(method, path)] = handler;
  }

  void HandleRequest(HTTPServerConnection& connection) const {
    const HTTPReceivedMessage& message = connection.Message();
    const Method method = message.Method() == "GET" ? Method::GET : Method::POST;  // TODO(dkorolev).
    const std::string& path = message.Path();  // TODO(dkorolev): Rename message.Path(), it's incorrect.
    const url::URL url = url::URL(path);
    const auto cit = routes_.find(std::make_pair(method, url.path));
    if (cit != routes_.end()) {
      Request request{url, connection, message};
      cit->second(std::forward<Request>(request));
    } else {
      ASSERT_TRUE(false) << "Method not implemented.";
    }
  }

  std::map<std::pair<Method, std::string>, std::function<void(Request&&)>> routes_;
};

class Server {
 public:
  Server(int port, bool& time_to_terminate, Routes routes)
      : port_(port),
        time_to_terminate_(time_to_terminate),
        routes_(routes),
        thread_(&Server::Code, this, Socket(port_)) {}

  ~Server() { HTTP(GET(strings::Printf("localhost:%d/killtestserver", port_))); }

 private:
  void Code(Socket socket) {
    while (!time_to_terminate_) {
      HTTPServerConnection connection(socket.Accept());
      routes_.HandleRequest(connection);
    }
  }

  const int port_;
  const bool& time_to_terminate_;
  const Routes routes_;
  std::thread thread_;
};

class TestHTTPServer_APIImpl {
 public:
  class Impl {
   public:
    Impl(int port) : server_(port, time_to_terminate_, PopulateRoutes(time_to_terminate_)) {}

   private:
    static Routes PopulateRoutes(bool& time_to_terminate) {
      Routes routes;
      routes[Method::GET]["/killtestserver"] = [&time_to_terminate](Request&& r) {
        r.connection.SendHTTPResponse("Roger that.");
        time_to_terminate = true;
      };
      routes[Method::GET]["/drip"] = [](Request&& r) {
        const size_t numbytes = atoi(r.url.query["numbytes"].c_str());
        Connection& c = r.connection.RawConnection();
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
        c.SendEOF();
      };
      routes[Method::GET]["/status/403"] =
          [](Request&& r) { r.connection.SendHTTPResponse("", HTTPResponseCode::Forbidden); };
      routes[Method::GET]["/get"] =
          [](Request&& r) { r.connection.SendHTTPResponse("{\"Aloha\": \"" + r.url.query["Aloha"] + "\"}\n"); };
      routes[Method::GET]["/user-agent"] = [](Request&& r) {
        // TODO(dkorolev): Add parsing User-Agent to Bricks' HTTP headers parser.
        // The response should be taken from the header, not hardcoded to be "Aloha User Agent".
        r.connection.SendHTTPResponse("Aloha User Agent");
      };
      routes[Method::GET]["/redirect-to"] = [](Request&& r) {
        HTTPHeadersType headers;
        headers.push_back(std::make_pair("Location", r.url.query["url"]));
        r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
      };
      routes[Method::GET]["/redirect-loop"] = [](Request&& r) {
        HTTPHeadersType headers;
        headers.push_back(std::make_pair("Location", "/redirect-loop-2"));
        r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
      };
      routes[Method::GET]["/redirect-loop-2"] = [](Request&& r) {
        HTTPHeadersType headers;
        headers.push_back(std::make_pair("Location", "/redirect-loop-3"));
        r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
      };
      routes[Method::GET]["/redirect-loop-3"] = [](Request&& r) {
        HTTPHeadersType headers;
        headers.push_back(std::make_pair("Location", "/redirect-loop"));
        r.connection.SendHTTPResponse("", HTTPResponseCode::Found, "text/html", headers);
      };
      routes[Method::POST]["/post"] = [](Request&& r) {
        ASSERT_TRUE(r.message.HasBody());
        r.connection.SendHTTPResponse("{\"data\": \"" + r.message.Body() + "\"}\n");
      };
      return routes;
    }

    bool time_to_terminate_ = false;
    Server server_;
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
}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_TEST_SERVER_CC_H
