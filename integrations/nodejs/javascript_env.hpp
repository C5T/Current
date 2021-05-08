#pragma once

#include "napi.h"

namespace current {
namespace javascript {
namespace impl {

// NOTE(dkorolev): There are only a limited number of places from which the NodeJS API can be used. The necessary,
// but not sufficient, condition is that in order to use the NodeJS API a valid `Napi::Env` must be present.
//
// To make following these rules both stricter (i.e. with more checks) and cleaner (i.e. no need to carry `env` around),
// the thread-local `JSEnvScopesStack` class is used, which is only updated through the lifetimes of `JSEnvScope`.
//
// This, among others, makes it easier to wrap C++ objects into JavaScript, as they can be "cast" automatically,
// as long as the thread-local `JSEnvScopesStack` has the valid `Napi::Env` on the top.
//
// The contents of this stack of node env-s is managed by the Current binding code, and it makes sure that the top
// of the stack contains the valid `Napi::Env` when and only when it is legal for the C++ code to interface with NodeJS.

class JSEnvScope;
class JSEnvScopesStack final {
 private:
  friend class JSEnvScope;
  static_assert(std::is_pointer<napi_env>::value, "`napi_env` should be a pointer.");
  std::vector<std::pair<napi_env, const JSEnvScope*>> envs_;

  void Push(napi_env env, const JSEnvScope* scope) { envs_.emplace_back(env, scope); }

  void Pop(const JSEnvScope* scope) {
    if (envs_.empty() || envs_.back().second != scope) {
      Napi::Error::Fatal("CurrentJSBinding", "Inconsistent state in `JSEnvScopesStack::Pop()`.");
    }
    envs_.pop_back();
  }

 public:
  JSEnvScopesStack() {}
  ~JSEnvScopesStack() {
    if (!envs_.empty()) {
      Napi::Error::Fatal("CurrentJSBinding", "Inconsistent state in `JSEnvScopesStack::~JSEnvScopesStack()`.");
    }
  }

  napi_env GetJSEnvOrFail() const {
    if (envs_.empty() || !envs_.back().first) {
      Napi::Error::Fatal("CurrentJSBinding", "Attempted to use JS-specific logic from where it is not allowed.");
    }
    return envs_.back().first;
  }

  inline static JSEnvScopesStack& ThreadLocalSingleton() {
    thread_local static JSEnvScopesStack impl;
    return impl;
  }
};

inline Napi::Env JSEnv() { return Napi::Env(JSEnvScopesStack::ThreadLocalSingleton().GetJSEnvOrFail()); }

class JSEnvScope final {
 public:
  explicit JSEnvScope(Napi::Env env) { JSEnvScopesStack::ThreadLocalSingleton().Push(env.operator napi_env(), this); }
  explicit JSEnvScope(std::nullptr_t) { JSEnvScopesStack::ThreadLocalSingleton().Push(nullptr, this); }
  ~JSEnvScope() { JSEnvScopesStack::ThreadLocalSingleton().Pop(this); }
};

}  // namespace impl
}  // namespace javascript
}  // namespace current
