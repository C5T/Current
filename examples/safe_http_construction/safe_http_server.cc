#include "../../blocks/http/api.h"
#include"../../bricks/dflags/dflags.h"

DEFINE_uint16(port, 8080, "The local port to use.");
DEFINE_bool(unsafe, false, "Set to illustrate the unsafe behavior.");

inline void RunUnsafe() {
  // This code does not do what the user expects, since listening on the port happens on a separate thread,
  // which means it is done asynchronously, so that a) the exception will be thrown in a different thread, and
  // 2) it will be thrown too late to be caught regardless.
  //
  // The output of the 2nd concurrent run of `./.current/safe_http_server --unsafe` would likely be:
  // $ ./.current/safe_http_server --unsafe
  // unsafe ok, listening
  // terminate called after throwing an instance of 'current::net::SocketBindException'
  //   what():  ... SocketBindException(static_cast<uint16_t>(bare_port))	8080
  // zsh: abort (core dumped)  ./.current/safe_http_server --unsafe
  //
  // This is why the recommended way to acquire the port in a try/catch block for the HTTP server
  // is via `current::net::AcquireLocalPort`, the return value of which is `std::move()`-d into `HTTP()`
  try {
    auto& http = HTTP(current::net::BarePort(FLAGS_port));
    auto scope = http.Register("/", [](Request r) { r("unsafe ok\n"); });
    std::cerr << "unsafe ok, listening" << std::endl;
    http.Join();
  } catch (current::net::SocketBindException const&) {
    // NOTE(dkorolev): This `catch` block will *not* be executed!
    // TODO(dkorolev): Maybe even remove this old `HTTP()` constructor, or at least mark it as deprecated.
    std::cerr << "unsafe can not listen, the local port is taken" << std::endl;
  }
}

inline void RunSafe() {
  // This is the correct way to ensure the `current::net::SocketBindException` is caught.
  // TODO(dkorolev): Now that I think about it, we may well make `RunUnsafe()` do the same thing under the hood.
  try {
    auto safe_port = current::net::AcquireLocalPort(FLAGS_port);
    auto& http = HTTP(std::move(safe_port));
 
    // Of course, the commented line below would also work. The above two are just for illustrative purposes.
    // auto& http = HTTP(current::net::AcquireLocalPort(FLAGS_port));

    auto scope = http.Register("/", [](Request r) { r("safe ok\n"); });
    std::cerr << "safe ok, listening" << std::endl;
    http.Join();
  } catch (current::net::SocketBindException const&) {
    // NOTE(dkorolev): This `catch` block will *not* be executed!
    // TODO(dkorolev): Maybe even remove this old `HTTP()` constructor, or at least mark it as deprecated.
    std::cerr << "safe can not listen, the local port is taken" << std::endl;
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);

  if (!FLAGS_unsafe) {
    RunSafe();
  } else {
    RunUnsafe();
  }
}
