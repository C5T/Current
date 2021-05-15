#include <chrono>

// This is the `current/intergrations/nodejs/javascript.hpp`, with the path to it set in `binding.gyp`,
// so that the node-based build and test, which can be run with just `make` from this directory, does the job.
#include "javascript.hpp"

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  using namespace current::javascript;

  JSEnvScope scope(env);

  // Some immediate values.
  exports["valueInt"] = CPP2JS(42);
  exports["valueDouble"] = CPP2JS(3.14);
  exports["valueString"] = CPP2JS("The Answer");
  exports["valueTrue"] = CPP2JS(true);
  exports["valueFalse"] = CPP2JS(false);
  exports["valueNull"] = CPP2JS(nullptr);
  exports["valueUndefined"] = CPP2JS(Undefined());

  // The simple case: Just return the sum.
  exports["cppSyncSum"] = CPP2JS([](int a, int b) { return a + b; });

  // Another simple case: Invoke the callback with the sum as the only argument.
  exports["cppSyncCallbackSum"] = CPP2JS([](int a, int b, JSScopedFunction f) { f(a + b); });

  // The asynchronous callback must be called from within the right place, where it's legal to call into JavaScript.
  exports["cppAsyncCallbackSum"] = CPP2JS([](int a, int b, JSFunction f) {
    JSAsync([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); }, [f, a, b]() { f(a + b); });
  });

  // The future can also be only set from the right place.
  exports["cppFutureSum"] = CPP2JS([](int a, int b) {
    JSPromise promise;
    JSAsync([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); },
            [promise, a, b]() { promise = a + b; });
    return promise;
  });

  // Check that `return nullptr;` returns `null` into JavaScript, and returning nothing returns `undefined`.
  exports["cppReturnsNull"] = CPP2JS([]() { return nullptr; });
  exports["cppReturnsUndefined"] = CPP2JS([]() {});
  exports["cppReturnsUndefinedII"] = CPP2JS([]() { return Undefined(); });

  // Note: The arguments here can be `JSFunction` or `JSScopedFunction`, as they are only called synchronously,
  // from the "main thread". In such a scenario, `JSScopedFunction` is preferred, as it has a lower overhead.
  exports["cppSyncCallbacksABA"] = CPP2JS([](JSScopedFunction f, JSScopedFunction g) {
    f(1);
    g(2);
    f(":three");
    // This weird way of returning from a synchronous function is unnecessary, but it is used here deliberately,
    // for the JavaScript tests for `cppSyncCallbacksABA` and `cppAsyncCallbacksABA` to be identical.
    JSPromise promise;
    promise = nullptr;
    return promise;
  });

  // Note: Unlike in `cppSyncCallbacksABA`, these functions should be of type `JSFunction`, not `JSScopedFunction`.
  exports["cppAsyncCallbacksABA"] = CPP2JS([](JSFunction f, JSFunction g) {
    JSPromise promise;
    JSAsync([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); },
            [f, g, promise]() {
              f("-test");
              g(":here:", nullptr, ":", 3.14, ":", true);
              f("-passed");
              promise = nullptr;
            });
    return promise;
  });

  // A "native" lambda can be "returned", and the magic behind the scenes will work its way.
  exports["cppWrapsFunction"] = CPP2JS([](int x, JSFunctionReturning<std::string> f) {
    // NOTE(dkorolev): Just `return`-ing a lambda compiles, but the JS env garbage-collects that function.
    // So, use the trick of not leaving the scope of the function.
    return f([x](int y) { return "Outer " + std::to_string(x) + ", inner " + std::to_string(y) + '.'; });
  });

  // A C++ code that calls back into JavaScript and analyzes the results of those calls.
  exports["cppGetsResultsOfJsFunctions"] = CPP2JS(
      [](JSScopedFunctionReturning<std::string> a, JSScopedFunctionReturning<std::string> b) { return a() + b(); });

  // A C++ code that calls back into JavaScript and analyzes the results of those calls.
  exports["cppGetsResultsOfJsFunctionsAsync"] =
      CPP2JS([](JSFunctionReturning<std::string> a, JSFunctionReturning<std::string> b) {
        JSPromise promise;
        JSAsync([]() { std::this_thread::sleep_for(std::chrono::milliseconds(50)); },
                [a, b, promise]() { promise = a() + b(); });
        return promise;
      });

  // Calling back JavaScript code multiple times, at arbitrary times.
  exports["cppMultipleAsyncCallsAtArbitraryTimes"] =
      CPP2JS([](JSFunctionReturning<bool> f_cond, JSFunctionReturning<void> f_print) {
        const auto is_odd_prime = [](int p) {
          for (int d = 3; d * d <= p; d += 2) {
            if ((p % d) == 0) {
              return false;
            }
          }
          return true;
        };

        JSPromise promise;

        JSAsyncEventLoop([f_cond, f_print, promise, is_odd_prime](JSAsyncEventLoopRunner run_in_event_loop) {
          // Demo that `run_in_event_loop` can take a function returning a `bool`.
          if (!run_in_event_loop([f_cond, f_print, promise]() {
                if (!f_cond(1) || !f_cond(2)) {
                  promise = 0;
                  return false;
                } else {
                  return true;
                }
              })) {
            return;
          }

          run_in_event_loop([f_print]() { f_print(2); });

          int odd_p = 3;
          int total_primes_found = 1;

          while (true) {
            std::this_thread::sleep_for(std::chrono::milliseconds(odd_p));  // Sleep progressively longer.
            const bool is_odd_p_prime = is_odd_prime(odd_p);
            if (!run_in_event_loop([f_cond, f_print, is_odd_p_prime, &odd_p, &total_primes_found, promise]() {
                  if (!f_cond(odd_p)) {
                    promise = total_primes_found;
                    return false;
                  }
                  if (is_odd_p_prime) {
                    ++total_primes_found;
                    f_print(odd_p);
                  }
                  if (!f_cond(++odd_p)) {
                    promise = total_primes_found;
                    return false;
                  }
                  ++odd_p;
                  return true;
                })) {
              return;
            }
          }
        });

        return promise;
      });

  return exports;
}

NODE_API_MODULE(example, Init)
