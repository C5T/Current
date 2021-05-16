## Current NodeJS Integration

This is the C++ part of Current's C++ <=> NodeJS integration. Please refer to the example in `current/examples/nodejs` for more details.

TL;DR:
* You would need to have `node-devel` installed in the system (C++ libs and headers).
* Your project would need to have `node-addon-api` and `bindings` installed via `npm`.
* Please refer to `current/examples/nodejs` for the reference `package*.json`, `binding.gyp`, and the command lines, in the `Makefile` there.

### Implementation Details

#### Headers

In this directory the header files are of `.hpp` extension. This is primarily to make sure they are excluded from Current's top-level `make check` target.

Also node that these header files are in C++14, even though Current itself is C++17. I was having troubles building C++17 with `node-gyp` cross-platform (things vary across `node-gyp`'s build scripts on Linux and macOS, so I decided to stick with C++14 for now, to be safe).

#### V8 Environments

When working with V8 from C++, it is important to only make calls into the V8 stack from where it is allowed. This affects any V8-related logic, including, but not limited to, calling JavaScript functions, resolving JavaScript promises, wrapping C++ objects into JavaScript, and even releasing JavaScript objects so that they can be garbage-collected by V8.

This requirement is rather strict and painful, so, in this integration code, the radical solution is to use the wrapper `JSEnvScopesStack` thread-local singleton, and a simple `JSEnv()` call to access the now-valid V8 context.

The very call to `JSEnv()` would fail, with a fatal error, if attempted to be made from the context where no valid environment is set.

The end user, however, would not need to use `JSEnv()`, or the `JSEnvScopesStack` at all, except for the magic `JSEnvScope scope(env);` line in the `Init` function of the user code -- to ensure that the exported functions and objects will be properly wrapped, within the environment `env` passed by the framework to the very `Init` function.

#### Lifetime

In this integration, JavaScript promises and JavaScript functions are wrapped into a `shared_ptr<>`-holding helper classes.

This ensures that `JSPromise` can be captured by value into C++ lambdas, and then just set from there, with `operator=()`. Same goes for the functions: the `JSFunction` instances can be captured by value into C++ lambdas, and called from there.

Keep in mind that both resolving JavaScript promises and calling JavaScript functions can only be done from where the V8 environment allows for this.

#### Sync and Async

If the C++ function is synchronous, i.e. it does work in one thread and then returns the value, then there is no need to worry about the validity of V8 environment, as the integration code takes care of this.

For asynchronous work, take a look at `JSAsync`. It takes two lambdas as the two arguments: the first lambda is a C++-only one, and the second one is called from within a valid V8 environment.

Thus, the C++ heavylifting can be done within the first lambda, and JavaScript functions can be called, and JavaScript promises can be resolved, from within the second one.

Also, in addition to `JSFunction` (which is `JSFunctionReturning<void>`), this integration also provides the `JSFunctionReference = JSFunctionReferenceReturning<void>` primitive, for C++ objects corresponding to C++ functions that do not need to cross the sync/async boundary. A `JSFunction` can be used instead of a `JSFunctionReference` anywhere, while, in async contexts, `JSFunction`, not `JSFunctionReference`, should be used. In simple terms, `JSFunctionReference` does not indicate to the V8 engine that the function may need to be invoked later, and thus it would not have the async call context created for itself.

Finally, while `JSAsync` performs one asynchronous operation, `JSAsyncLoop` makes it possible for the C++ code to intertwine with the JavaScript event loop. Use it when JavaScript functions need to be called from within C++, at arbitrary times, as many times as necessary.

#### Lambdas and Traits

While this integration exports the `CPP2JS` and `JS2CPP` functions, the user would hardly need to use them.

In the "user space" `Init` code, just pass `exports["myFunctionName"] = CPP2JS([](...) { ... });`, and the framework would automatically take care of converting the arguments back and forth.

In addition to the "primitive" types (numbers, strings, `null`-s that are mapped to `nullptr`-s in C++), the instrumentation also handles `JSFunction`-s and `JSPromise`-s correctly. The C++ code can natively call, via `operator()`, the `JSFunction`-s that are passed as parameters from JavaScript. All the arguments would be automatically converted from their C++ representations to the JavaScript ones. Use `JSFunctionReturning<T>`, as the argument type for the lambda, to have the return value of the JavaScript function converted to the respective C++ type.

Of course, as JavaScript is not a strongly typed language and the integration code can not guarantee the user would pass in the arguments of the right type, such an attempt would result in a runtime error.

#### More

Here's a non-exhaustive list of other features that are either done but not documented yet, or work in progress as of this very moment:

* Support for `vector<uint8_t>` <=> JavaScript mapping.
* Support for `int64` / `BigInt`, as JavaScript's integers are not full range.
* Current-native bindings to JavaScript objects and arrays.
* C++ functions "overloading" for JavaScript.
