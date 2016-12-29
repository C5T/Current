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

namespace current {
namespace http {

// LCOV_EXCL_START
struct InvalidHandlerPathException : current::net::HTTPException {
  using current::net::HTTPException::HTTPException;
};
// LCOV_EXCL_STOP

struct PathDoesNotStartWithSlash : InvalidHandlerPathException {
  using InvalidHandlerPathException::InvalidHandlerPathException;
};

struct PathEndsWithSlash : InvalidHandlerPathException {
  using InvalidHandlerPathException::InvalidHandlerPathException;
};

struct PathContainsInvalidCharacters : InvalidHandlerPathException {
  using InvalidHandlerPathException::InvalidHandlerPathException;
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
      r.connection.SendHTTPResponse(current::net::DefaultMethodNotAllowedMessage(),
                                    HTTPResponseCode.MethodNotAllowed,
                                    current::net::constants::kDefaultHTMLContentType);
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
  using HTTPRoutesScopeEntry = HTTPRoutesScope;  // TODO(dkorolev): Eliminate.

  // The philosophy of Register(path, handler):
  //
  // 1) Always accept `handler` by reference. Require the reference to be non-const.
  //    (To avoid accidentally passing in a temporary, etc. Register("/foo", Foo())`.
  // 2) Unless the passed in handler can be cast to an `std::function<void(Request)>`,
  //    in which case it is copied. (To make sure passing in lambdas just works).
  //
  // NOTE: The implementation of the above logic requires two separate signatures.
  //       An xvalue reference won't do it. Trust me, I've tried, and it results in horrible bugs. -- D.K.
  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt, typename F>
  HTTPRoutesScopeEntry Register(const std::string& path,
                                const URLPathArgs::CountMask path_args_count_mask,
                                F& handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(path, [&handler](Request r) { handler(std::move(r)); }, path_args_count_mask, POLICY);
  }

  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt>
  HTTPRoutesScopeEntry Register(const std::string& path,
                                const URLPathArgs::CountMask path_args_count_mask,
                                std::function<void(Request)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(path, handler, path_args_count_mask, POLICY);
  }

  // Two argument version registers handler with no URL path arguments.
  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt, typename F>
  HTTPRoutesScopeEntry Register(const std::string& path, F& handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(
        path, [&handler](Request r) { handler(std::move(r)); }, URLPathArgs::CountMask::None, POLICY);
  }
  template <ReRegisterRoute POLICY = ReRegisterRoute::ThrowOnAttempt>
  HTTPRoutesScopeEntry Register(const std::string& path, std::function<void(Request)> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    return DoRegisterHandler(path, handler, URLPathArgs::CountMask::None, POLICY);
  }

  void UnRegister(const std::string& path,
                  const URLPathArgs::CountMask path_args_count_mask = URLPathArgs::CountMask::None) {
    std::lock_guard<std::mutex> lock(mutex_);
    URLPathArgs::CountMask mask = URLPathArgs::CountMask::None;  // `None` == 1 == (1 << 0).
    for (size_t i = 0; i <= URLPathArgs::MaxArgsCount; ++i, mask <<= 1) {
      if ((path_args_count_mask & mask) == mask) {
        auto it1 = handlers_.find(path);
        if (it1 == handlers_.end()) {
          CURRENT_THROW(HandlerDoesNotExistException(path));
        }
        auto& map = it1->second;
        auto it2 = map.find(i);
        if (it2 == map.end()) {
          CURRENT_THROW(HandlerDoesNotExistException(path));
        }
        map.erase(it2);
        if (map.empty()) {
          // Maintain the value of `PathHandlersCount()` invariant.
          handlers_.erase(it1);
        }
      }
    }
  }

  HTTPRoutesScope ServeStaticFilesFrom(const std::string& dir, const std::string& route_prefix = "/") {
    HTTPRoutesScope scope;
    current::FileSystem::ScanDir(
        dir,
        [this, &dir, &route_prefix, &scope](const std::string& file) {
          const std::string content_type(current::net::GetFileMimeType(file, ""));
          if (!content_type.empty()) {
            // TODO(dkorolev): Wrap keeping file contents into a singleton
            // that keeps a map from a (SHA256) hash to the contents.
            auto static_file_server = std::make_unique<StaticFileServer>(
                current::FileSystem::ReadFileAsString(current::FileSystem::JoinPath(dir, file)), content_type);
            scope += Register(route_prefix + file, *static_file_server);
            static_file_servers_.push_back(std::move(static_file_server));
          } else {
            CURRENT_THROW(current::net::CannotServeStaticFilesOfUnknownMIMEType(file));
          }
        });
    return scope;
  }

  size_t PathHandlersCount() const {
    // NOTE: The total number of handlers is no longer an interesting measure.
    //       Just return the number of distinct paths, which may be path prefixes.
    std::lock_guard<std::mutex> lock(mutex_);
    return handlers_.size();
  }

 private:
  void FindHandler(const std::string& path,
                   std::function<void(Request)>& output_handler,
                   URLPathArgs& output_url_args) {
    // Just `return` is safe. Uninitialized `output_handler` would be interpreted as "no handler found".
    // LCOV_EXCL_START
    if (path.empty()) {
      std::cerr << "HTTP: path is empty.\n";
      return;
    }
    if (path[0] != '/') {
      std::cerr << "HTTP: path does not start with a slash.\n";
      return;
    }
    // LCOV_EXCL_STOP

    // Work with the path by trying to match the full one, then the one with the last component removed,
    // then the one with two last components removed, etc.
    // As a component is removed, it's removed from `base_path` and is added to `url_args`.
    std::string& remaining_path = output_url_args.base_path;
    remaining_path = path;

    // Friendly reminder: `remaining_path[0]` is guaranteed to be '/'.
    while (true) {
      while (remaining_path.length() > 1 && remaining_path.back() == '/') {
        remaining_path.resize(remaining_path.length() - 1);
      }

      const auto cit = handlers_.find(remaining_path);
      if (cit != handlers_.end()) {
        const auto cit2 = cit->second.find(output_url_args.size());
        if (cit2 != cit->second.end()) {
          output_handler = cit2->second;
          return;
        }
      }

      if (remaining_path == "/") {
        break;
      }

      const size_t arg_begin_index = remaining_path.rfind('/') + 1;
      output_url_args.add(remaining_path.substr(arg_begin_index));
      remaining_path.resize(arg_begin_index);
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
        URLPathArgs url_path_args;
        {
          // TODO(dkorolev): Read-write lock for performance?
          std::lock_guard<std::mutex> lock(mutex_);
          FindHandler(connection->HTTPRequest().URL().path, handler, url_path_args);
        }
        if (handler) {
          // OK, here's the tricky part with error handling and exceptions in this multithreaded world.
          // * On the one hand, the connection should be std::move-d into the request,
          //   since it might end up being served in another thread, via a message queue, etc.
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
            handler(Request(std::move(connection), url_path_args));
          } catch (const current::Exception& e) {  // LCOV_EXCL_LINE
            // WARNING: This `catch` is really not sufficient, it just logs a message
            // if a user exception occurred in the same thread that ran the handler.
            // DO NOT COUNT ON IT.
            std::cerr << "HTTP route failed in user code: " << e.what() << '\n';  // LCOV_EXCL_LINE
          }
        } else {
          connection->SendHTTPResponse(current::net::DefaultFourOhFourMessage(),
                                       HTTPResponseCode.NotFound,
                                       current::net::constants::kDefaultHTMLContentType);
        }
      } catch (const current::net::HTTPPayloadTooLarge&) {
        // The `HTTPPayloadTooLarge` situation, if emerged, is already handled with an "413 ENTITY TOO LARGE" response.
      } catch (const current::net::EmptySocketException&) {  // LCOV_EXCL_LINE
        // Silently discard errors if no data was sent in.
      } catch (const current::Exception& e) {  // LCOV_EXCL_LINE
        // TODO(dkorolev): More reliable logging.
        std::cerr << "HTTP route failed: " << e.what() << '\n';  // LCOV_EXCL_LINE
      }
    }
  }

  HTTPRoutesScopeEntry DoRegisterHandler(const std::string& path,
                                         std::function<void(Request)> handler,
                                         const URLPathArgs::CountMask path_args_count_mask,
                                         const ReRegisterRoute policy) {
    // LCOV_EXCL_START
    if (static_cast<uint16_t>(path_args_count_mask) == 0) {
      return HTTPRoutesScopeEntry();
    }

    if (path.empty() || path[0] != '/') {
      CURRENT_THROW(PathDoesNotStartWithSlash("HTTP path does not start with a slash: `" + path + "`."));
    }
    if (path != "/" && path[path.length() - 1] == '/') {
      CURRENT_THROW(PathEndsWithSlash("HTTP path ends with slash: `" + path + "`."));
    }
    if (!URL::IsPathValidToRegister(path)) {
      CURRENT_THROW(PathContainsInvalidCharacters("HTTP path contains invalid characters: `" + path + "`."));
    }
    // LCOV_EXCL_STOP

    {
      // Step 1: Confirm the request is valid.
      const auto handlers_per_path_iterator = handlers_.find(path);
      URLPathArgs::CountMask mask = URLPathArgs::CountMask::None;  // `None` == 1 == (1 << 0).
      for (size_t i = 0; i <= URLPathArgs::MaxArgsCount; ++i, mask = mask << 1) {
        if ((path_args_count_mask & mask) == mask) {
          if (handlers_per_path_iterator == handlers_.end() ||
              handlers_per_path_iterator->second.find(i) == handlers_per_path_iterator->second.end()) {
            // No such handler. Throw if trying to "Update" it.
            if (policy == ReRegisterRoute::SilentlyUpdateExisting) {
              CURRENT_THROW(HandlerDoesNotExistException(path));
            }
          } else {
            // Handler already exists. Throw if not in "Update" mode.
            if (policy != ReRegisterRoute::SilentlyUpdateExisting) {
              CURRENT_THROW(HandlerAlreadyExistsException(path));
            }
          }
        }
      }
    }

    {
      // Step 2: Update.
      auto& handlers_per_path = handlers_[path];
      URLPathArgs::CountMask mask = URLPathArgs::CountMask::None;  // `None` == 1 == (1 << 0).
      for (size_t i = 0; i <= URLPathArgs::MaxArgsCount; ++i, mask = mask << 1) {
        if ((path_args_count_mask & mask) == mask) {
          handlers_per_path[i] = handler;
        }
      }
    }

    if (policy == ReRegisterRoute::SilentlyUpdateExisting) {
      return HTTPRoutesScopeEntry();
    } else {
      return HTTPRoutesScopeEntry([this, path, path_args_count_mask]() { UnRegister(path, path_args_count_mask); });
    }
  }

  HTTPServerPOSIX() = delete;

  std::atomic_bool terminating_;
  const int port_;
  std::thread thread_;

  // TODO(dkorolev): Look into read-write mutexes here.
  mutable std::mutex mutex_;

  std::map<std::string, std::map<size_t, std::function<void(Request)>>> handlers_;
  std::vector<std::unique_ptr<StaticFileServer>> static_file_servers_;
};

}  // namespace http
}  // namespace current

#endif  // BLOCKS_HTTP_IMPL_POSIX_SERVER_H
