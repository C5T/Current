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

// TODO(dkorolev): Handle empty POST body. Add a test for it.
// TODO(dkorolev): Support receiving body via POST requests. Add a test for it.

#ifndef BRICKS_NET_API_POSIX_SERVER_H
#define BRICKS_NET_API_POSIX_SERVER_H

#include <atomic>
#include <string>
#include <map>
#include <memory>
#include <thread>
#include <iostream>  // TODO(dkorolev): More robust logging here.

#include "../types.h"

#include "../../exceptions.h"
#include "../../http/http.h"
#include "../../url/url.h"

#include "../../../strings/printf.h"

namespace bricks {
namespace net {
namespace api {

struct HandlerAlreadyExistsException : HTTPException {
  explicit HandlerAlreadyExistsException(const std::string& what) { SetWhat(what); }
};

struct HandlerDoesNotExistException : HTTPException {
  explicit HandlerDoesNotExistException(const std::string& what) { SetWhat(what); }
};

// The only parameter to be passed to HTTP handlers.
struct Request final {
  std::unique_ptr<HTTPServerConnection> connection_placeholder;
  const url::URL& url;
  HTTPServerConnection& connection;
  const HTTPReceivedMessage& message;

  Request(const url::URL& url, std::unique_ptr<HTTPServerConnection>&& connection)
      : connection_placeholder(std::move(connection)),
        url(url),
        connection(*connection_placeholder.get()),
        message(connection_placeholder->Message()) {}

  // It is essential to move `connection_placeholder` so that the socket outlives the destruction of `rhs`.
  Request(Request&& rhs)
      : connection_placeholder(std::move(rhs.connection_placeholder)),
        url(rhs.url),
        connection(*connection_placeholder.get()),
        message(connection_placeholder->Message()) {}

  Request() = delete;
  Request(const Request&) = delete;
  void operator=(const Request&) = delete;
  void operator=(Request&&) = delete;
};

// HTTP server bound to a specific port.
class HTTPServerPOSIX final {
 public:
  // The constructor starts listening on the specified port.
  // Since instances of `HTTPServerPOSIX` are created via a singleton,
  // a listening thread will only be created once per port, on the first access to that port.
  explicit HTTPServerPOSIX(int port)
      : terminating_(false), port_(port), thread_(&HTTPServerPOSIX::Thread, this, Socket(port)) {}

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
      Connection(ClientSocket("localhost", port_)).BlockingWrite("GET /healthz HTTP/1.1\r\n\r\n").SendEOF();
    } catch (const bricks::Exception&) {
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

  // NOTE: Should not pass in `std::function<void(Request&&)>` here, since it would copy the handler,
  // while a better design is to keep using the reference when the reference is passed in.
  // TODO(dkorolev): Change string into proper URL-fragment-matching handler in Register() and UnRegister().
  template <typename F>
  void Register(const std::string& path, F&& handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handlers_.find(path) != handlers_.end()) {
      BRICKS_THROW(HandlerAlreadyExistsException(path));
    }
    handlers_[path] = [&handler](Request&& request) { handler(std::forward<Request>(request)); };
  }

  void UnRegister(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (handlers_.find(path) == handlers_.end()) {
      BRICKS_THROW(HandlerDoesNotExistException(path));
    }
    handlers_.erase(path);
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
  void Thread(Socket socket) {
    // TODO(dkorolev): Benchmark QPS.
    while (!terminating_) {
      try {
        std::unique_ptr<HTTPServerConnection> connection(new HTTPServerConnection(socket.Accept()));
        if (terminating_) {
          break;
        }
        const std::string& path = connection->Message().Path();
        const url::URL url(path);
        std::function<void(Request && )> handler;
        {
          // TODO(dkorolev): Read-write lock for performance?
          std::lock_guard<std::mutex> lock(mutex_);
          const auto cit = handlers_.find(url.path);
          if (cit != handlers_.end()) {
            handler = cit->second;
          }
        }
        if (handler) {
          // TODO(dkorolev): Properly handle the shutdown case when the handler spawns another thread.
          handler(Request(url, std::move(connection)));
        } else {
          connection->SendHTTPResponse("", HTTPResponseCode::NotFound);
        }
      } catch (const std::exception& e) {  // LCOV_EXCL_LINE
        // TODO(dkorolev): More reliable logging.
        std::cerr << "HTTP route failed: " << e.what() << "\n";  // LCOV_EXCL_LINE
      }
    }
  }

  HTTPServerPOSIX() = delete;

  std::atomic_bool terminating_;
  const int port_;
  std::thread thread_;

  // TODO(dkorolev): Look into read-write mutexes here.
  mutable std::mutex mutex_;

  std::map<std::string, std::function<void(Request&&)>> handlers_;
};

}  // namespace api
}  // namespace net
}  // namespace bricks

#endif  // BRICKS_NET_API_POSIX_SERVER_H
