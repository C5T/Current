/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

// TODO(dkorolev): Refactor the code to disallow BODY-less POST requests.
// TODO(dkorolev): Support receiving body via POST requests. Add a test for it.

#ifndef BLOCKS_HTTP_IMPL_POSIX_SERVER_H
#define BLOCKS_HTTP_IMPL_POSIX_SERVER_H

#include <atomic>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <iostream>  // TODO(dkorolev): More robust logging here.

#include "../types.h"
#include "../request.h"

#include "../../URL/url.h"

#include "../../../Bricks/net/exceptions.h"
#include "../../../Bricks/net/http/http.h"
#include "../../../Bricks/time/chrono.h"
#include "../../../Bricks/strings/printf.h"
#include "../../../Bricks/util/accumulative_scoped_deleter.h"

namespace blocks {

struct InvalidHandlerPathException : current::net::HTTPException {
  using current::net::HTTPException::HTTPException;
};

struct HandlerAlreadyExistsException : current::net::HTTPException {
  using current::net::HTTPException::HTTPException;
};

struct HandlerDoesNotExistException : current::net::HTTPException {
  using current::net::HTTPException::HTTPException;
};

// Helper to serve a static file.
// TODO(dkorolev): Expose it externally under a better name, and add a comment/example.
struct StaticFileServer {
  std::string body;
  std::string content_type;
  explicit StaticFileServer(const std::string& body, const std::string& content_type)
      : body(body), content_type(content_type) {}
  void operator()(Request r) {
    if (r.method == "GET") {
      r.connection.SendHTTPResponse(body, HTTPResponseCode.OK, content_type);
    } else {
      r.connection.SendHTTPResponse(
          current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed, "text/html");
    }
  }
};

// HTTP server bound to a specific port.
class HTTPServerPOSIX final {
 public:
  // The constructor starts listening on the specified port.
  // Since instances of `HTTPServerPOSIX` are created via a singleton,
  // a listening thread will only be created once per port, on the first access to that port.
  explicit HTTPServerPOSIX(int port)
      : terminating_(false), port_(port), thread_(&HTTPServerPOSIX::Thread, this, current::net::Socket(port)) {}

  // The destructor closes the socket.
  // Note that the destructor will only be run on the shutdown of the binary,
  // unregistering all handlers will still keep the listening thread up, and it will serve 404-s.
  ~HTTPServerPOSIX() {
    terminating_ = true;
    // Notify the server thread that it should terminate.
    // Effectively, call `HTTP(GET("/healthz"))`, but in a way that avoids client <=> server dependency.
    // LCOV_EXCL_START
    try {
      // TODO(dkorolev): This should always use the POSIX implemenation of the client, nothing fancier.
      // It is a safe call, since the server itself is POSIX, so the architecture we are on is POSIX-friendly.
      current::net::Connection(current::net::ClientSocket("localhost", port_))
          .BlockingWrite("GET /healthz HTTP/1.1\r\n\r\n", true);
    } catch (const current::Exception&) {
      // It is guaranteed that after `terminated_` is set the server will be terminated on the next request,
      // but it might so happen that that terminating request will happen between `terminating_ = true`
      // and the consecutive request. Which is perfectly fine, since it implies that the server has terminated.
    }
    // LCOV_EXCL_STOP
    // Wait for the thread to terminate.
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  // The bare `Join()` method is only used by small scripts to run the server indefinitely,
  // instead of `while(true)`
  // LCOV_EXCL_START
  void Join() {
    thread_.join();  // May throw.
  }
  // LCOV_EXCL_STOP

  // Scoped de-registerer of routes, of its own type.
  struct ScopedRegistererDifferentiator {};
  using HTTPRoutesScope = current::AccumulativeScopedDeleter<ScopedRegistererDifferentiator>;
  using HTTPRoutesScopeEntry = current::AccumulativeScopedDeleter<ScopedRegistererDifferentiator, false>;

  struct HandlerWithParams {
    std::function<void(Request)> handler;
    URLPathParams::Count param_count;
    HandlerWithParams() = default;
    explicit HandlerWithParams(std::function<void(Request)> handler,
                               URLPathParams::Count param_count = URLPathParams::Count::None)
        : handler(handler), param_count(param_count) {}
  };
  // The philosophy of Register(path, handler):
  // * Pass `handler` by value to make its copy.
  //   This is done for lambdas and std::function<>-s.
  //   The lifetime of a copy is thus governed by the API.
  // * Pass `handler` by pointer to use the handler via pointer.
  //   This allows using passed in objects without making a copy of them.
  //   The lifetime of the object is then up to the user.
  // Justification: `Register("/foo", FooInstance())` has no way of knowing how long should `FooInstance` live.
  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt>
  HTTPRoutesScopeEntry Register(const std::string& path,
                                std::function<void(Request)> handler,
                                const URLPathParams::Count path_param_count = URLPathParams::Count::None) {
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(path, handler, path_param_count, POLICY);
  }

  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt, typename F>
  HTTPRoutesScopeEntry Register(const std::string& path,
                                F* ptr_to_handler,
                                const URLPathParams::Count path_param_count = URLPathParams::Count::None) {
    // TODO(dkorolev): Add a scoped version of registerers.
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(path,
                             [ptr_to_handler](Request request) { (*ptr_to_handler)(std::move(request)); },
                             path_param_count,
                             POLICY);
  }

  void UnRegister(const std::string& path) {
    // TODO(dkorolev): Add a scoped version of registerers.
    std::lock_guard<std::mutex> lock(mutex_);
    if (handlers_.find(path) == handlers_.end()) {
      CURRENT_THROW(HandlerDoesNotExistException(path));
    }
    handlers_.erase(path);
  }

  void ServeStaticFilesFrom(const std::string& dir, const std::string& route_prefix = "/") {
    // TODO(dkorolev): Add a scoped version of registerers.
    current::FileSystem::ScanDir(
        dir,
        [this, &dir, &route_prefix](const std::string& file) {
          const std::string content_type(current::net::GetFileMimeType(file, ""));
          if (!content_type.empty()) {
            // TODO(dkorolev): Wrap keeping file contents into a singleton
            // that keeps a map from a (SHA256) hash to the contents.
            Register(route_prefix + file,
                     new StaticFileServer(
                         current::FileSystem::ReadFileAsString(current::FileSystem::JoinPath(dir, file)),
                         content_type));
          } else {
            CURRENT_THROW(current::net::CannotServeStaticFilesOfUnknownMIMEType(file));
          }
        });
  }

  void ResetAllHandlers() {
    std::lock_guard<std::mutex> lock(mutex_);
    handlers_.clear();
  }

  size_t HandlersCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return handlers_.size();
  }

 private:
  void FindHandler(const std::string& path,
                   std::function<void(Request)>& dest_handler,
                   URLPathParams& dest_path_params) {
    assert(!path.empty());
    assert(path[0] == '/');
    std::string& remaining_path = dest_path_params.base_path;
    remaining_path = path;

    auto MatchPathParamCounts =
        [](const URLPathParams::Count handler_param_count, const size_t path_param_count) {
          if (path_param_count == 0) {
            return (handler_param_count == URLPathParams::Count::None ||
                    handler_param_count == URLPathParams::Count::Any);
          } else {
            return static_cast<bool>(
                handler_param_count &
                static_cast<URLPathParams::Count>(path_param_count ? (1 << (path_param_count - 1)) : 0));
          }
        };

    while (true) {
      const auto cit = handlers_.find(remaining_path);
      if (cit != handlers_.end() && MatchPathParamCounts(cit->second.param_count, dest_path_params.size())) {
        dest_handler = cit->second.handler;
        return;
      } else {
        if (remaining_path == "/") {
          return;
        }
        const size_t rlen = remaining_path.length();
        assert(rlen > 1);
        const size_t last_slash_pos = remaining_path.rfind('/');
        if (last_slash_pos > 0) {
          if (last_slash_pos != rlen - 1) {
            dest_path_params.add(remaining_path.substr(last_slash_pos + 1));
          }
          remaining_path.resize(last_slash_pos);
        } else {
          dest_path_params.add(remaining_path.substr(1));
          remaining_path.resize(1);
        }
      }
    }
  }

  void Thread(current::net::Socket socket) {
    // TODO(dkorolev): Benchmark QPS.
    while (!terminating_) {
      try {
        std::unique_ptr<current::net::HTTPServerConnection> connection(
            new current::net::HTTPServerConnection(socket.Accept()));
        if (terminating_) {
          // Already terminating. Will not send the response, and this
          // lack of response should not result in an exception.
          connection->DoNotSendAnyResponse();
          break;
        }
        std::function<void(Request)> handler;
        URLPathParams url_path_params;
        {
          // TODO(dkorolev): Read-write lock for performance?
          std::lock_guard<std::mutex> lock(mutex_);
          FindHandler(connection->HTTPRequest().URL().path, handler, url_path_params);
        }
        if (handler) {
          // OK, here's the tricky part with error handling and exceptions in this multithreaded world.
          // * On the one hand, connection should be std::move-d into the request,
          //   since it might end up being served in another thread, via a message queue, etc..
          //   Thus, the user code is responsible for closing the connection.
          //   Not to mention that the std::move-d away connection can easily outlive this scope.
          // * On the other hand, if an exception occurs in user code, we need to return a 500,
          //   which should obviously happen before the connection object is destructed.
          //   This seems like a good reason to not std::move it away, or move it away with some flag,
          //   but I thought hard of it, and don't think it's a good choice -- D.K.
          //
          // Solution: Do nothing here. No matter how tempting it is, it won't work across threads. Period.
          //
          // The implementation of HTTP connection will return an "INTERNAL SERVER ERROR"
          // if no response was sent. That's what the user gets. In debugger, they can put a breakpoint there
          // and see what caused the error.
          //
          // It is the job of the user of this library to ensure no exceptions leave their code.
          // In practice, a top-level try-catch for `const current::Exception& e` is good enough.
          try {
            handler(Request(std::move(connection), url_path_params));
          } catch (const current::Exception& e) {  // LCOV_EXCL_LINE
            // WARNING: This `catch` is really not sufficient, it just logs a message
            // if a user exception occurred in the same thread that ran the handler.
            // DO NOT COUNT ON IT.
            std::cerr << "HTTP route failed in user code: " << e.what() << "\n";  // LCOV_EXCL_LINE
          }
        } else {
          connection->SendHTTPResponse(
              current::net::DefaultFourOhFourMessage(), HTTPResponseCode.NotFound, "text/html");
        }
      } catch (const current::net::EmptySocketException&) {
        // Silently discard errors if no data was sent in.
      } catch (const current::Exception& e) {  // LCOV_EXCL_LINE
        // TODO(dkorolev): More reliable logging.
        std::cerr << "HTTP route failed: " << e.what() << "\n";  // LCOV_EXCL_LINE
      }
    }
  }

  HTTPRoutesScopeEntry DoRegisterHandler(const std::string& path,
                                         std::function<void(Request)> handler,
                                         const URLPathParams::Count path_param_count,
                                         const ReRegisterRoute policy) {
    if (path.empty() || path[0] != '/' || (path.length() > 1 && path[path.length() - 1] == '/') ||
        !URL::IsValidPath(path)) {
      CURRENT_THROW(InvalidHandlerPathException("Invalid handler path '" + path + "'"));
    }

    // Check if handler with exactly the same path exists and overwrite it if policy allows.
    if (handlers_.find(path) != handlers_.end()) {
      if (policy == ReRegisterRoute::SilentlyUpdate) {
        handlers_[path] = HandlerWithParams(handler, path_param_count);
        return HTTPRoutesScopeEntry();
      } else {
        CURRENT_THROW(HandlerAlreadyExistsException(path));
      }
    }

    handlers_[path] = HandlerWithParams(handler, path_param_count);
    return HTTPRoutesScopeEntry([this, path]() { UnRegister(path); });
  }

  HTTPServerPOSIX() = delete;

  std::atomic_bool terminating_;
  const int port_;
  std::thread thread_;

  // TODO(dkorolev): Look into read-write mutexes here.
  mutable std::mutex mutex_;

  std::map<std::string, HandlerWithParams> handlers_;
};

}  // namespace blocks

using blocks::Request;

#endif  // BLOCKS_HTTP_IMPL_POSIX_SERVER_H
