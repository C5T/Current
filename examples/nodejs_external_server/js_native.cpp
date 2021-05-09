// This is the `current/intergrations/nodejs/javascript.hpp`, with the path to it set in `binding.gyp`,
// so that the node-based build and test, which can be run with just `make` from this directory, does the job.
#include "javascript.hpp"

// And this is header for the "external" lib, that is linked with this "native" one.
#include <iostream>

#include "external/js_external.h"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  using namespace current::javascript;

  JSEnvScope scope(env);

  int base_port = 3000;

  exports["spawnServer"] = CPP2JS([&base_port](JSFunction registerers_callback) {
    const int port = ++base_port;

    JSPromise promise;

    // NOTE(dkorolev): The safe pattern is to capture `[=]` outside the event loop, `[&]` within in. This:
    // 1) Captures the JavaScript objects (functions, promises) by copy, preserving their lifetime / refcounts, and
    // 2) Captures whatever has to be run within `run_in_event_loop` by reference, because the runs are synchronous.
    JSAsyncEventLoop([=](JSAsyncEventLoopRunner run_in_event_loop) {
      // NOTE(dkorolev): The external server is a `unique_ptr<>` because its destruction should happen within (!)
      // the JavaScript event loop, as its destructor releases the ref-counts of JavaScript callbacks. Hard life, sigh.
      auto external_server = std::make_unique<ExternalServer>(port);

      // Call the JavaScript registerer callback, and wrap its callbacks as value-captured C++ lambdas.
      // This guarantees the lifetime of those `JSFunction`-s would outlive this very `run_in_event_loop()` call.
      run_in_event_loop([&]() {
        registerers_callback(port, [&](std::string route, JSFunction cb) {
          std::cout << "[ outer c++ ] Registering the callback '" << route << "' on port " << port << "." << std::endl;
          external_server->Register(route, [=](std::string route) {
            std::string retval;
            run_in_event_loop(
                [&]() { cb(std::move(route), [&retval](std::string route) { retval = std::move(route); }); });
            return retval;
          });
          std::cout << "[ outer c++ ] Registered the callback '" << route << "' on port " << port << "." << std::endl;
        });
      });

      std::cout << "[ outer c++ ] Waiting until the server on port " << port << " is 'DELETE'-d." << std::endl;
      external_server->WaitUntilDELETEd();
      std::cout << "[ outer c++ ] Done waiting until the server on port " << port << " is 'DELETE'-d." << std::endl;

      run_in_event_loop([&]() {
        // NOTE(dkorolev): Somewhat counterintuitively, it is important to destroy the instance of the externa server
        // from within the JavaScript event look. Because it destroys the JavaScript callbacks (captured by value
        // into C++ lambdas), and, as the refcounts of those callbacks drops to zero, they are destroyed on the JS side,
        // which, in its turn, can only happen from within the JavaScript event loop. Life is hard indeed.
        external_server = nullptr;
      });

      run_in_event_loop([&]() { promise = port; });
    });
    return promise;
  });

  return exports;
}

NODE_API_MODULE(example, Init)
