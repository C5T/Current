#pragma once

// clang-format off

#include "napi.h"  // NOTE(dkorolev): Keep the `napi.h` header file first.

#include "javascript_async.hpp"
#include "javascript_env.hpp"
#include "javascript_function.hpp"
#include "javascript_promise.hpp"
#include "javascript_cpp2js.hpp"
#include "javascript_js2cpp.hpp"
#include "javascript_function_cont.hpp"

// clang-format on

namespace current {
namespace javascript {

using impl::CPP2JS;
using impl::JS2CPP;

using impl::JSFunction;
using impl::JSFunctionReturning;
using impl::JSScopedFunction;
using impl::JSScopedFunctionReturning;

using impl::JSAsync;
using impl::JSPromise;

using impl::JSEnvScope;

}  // namespace current::javascript
}  // namespace current
