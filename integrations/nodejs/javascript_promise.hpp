#pragma once

#include "javascript_cpp2js.hpp"
#include "javascript_env.hpp"

namespace current {
namespace javascript {
namespace impl {

// The `JSPromise` contains a `shared_ptr<>` internally, so that it can be passed around and copy-captured into lambdas.
class JSPromise final {
 private:
  class JSPromiseImpl final {
   private:
    Napi::Promise::Deferred napi_promise_;

   public:
    explicit JSPromiseImpl() : napi_promise_(JSEnv()) {}
    Napi::Promise GetNapiPromise() { return napi_promise_.Promise(); }

    // NOTE(dkorolev): This method is `const` so that just capturing `JSPromise` by value into a lambda is sufficient.
    void DoResolve(Napi::Value value) const { napi_promise_.Resolve(value); }
  };

  std::shared_ptr<JSPromiseImpl> impl_;

 public:
  explicit JSPromise() : impl_(std::make_shared<JSPromiseImpl>()) {}
  Napi::Promise GetNapiPromise() const { return impl_->GetNapiPromise(); }

  // NOTE(dkorolev): This method is `const` so that just capturing `JSPromise` by value into a lambda is sufficient.
  template <typename T>
  void operator=(T&& x) const {
    impl_->DoResolve(CPP2JS(std::forward<T>(x)));
  }
};

template <>
struct CPP2JSImpl<false, JSPromise> {
  using type = Napi::Promise;
  static type DoIt(JSPromise x) { return x.GetNapiPromise(); }
};

}  // namespace impl
}  // namespace javascript
}  // namespace current
