#pragma once

#include "javascript_env.hpp"

namespace current {
namespace javascript {
namespace impl {

// `JSAsync` does some C++ work in a external thread, and then calls into a JavaScript-friendly scope.
template <class F, class G>
class JSAsyncImpl final : public Napi::AsyncWorker {
 private:
  F f_;
  G g_;

 public:
  JSAsyncImpl(F&& f, G&& g) : AsyncWorker(JSEnv()), f_(std::forward<F>(f)), g_(std::forward<G>(g)) {}

  void Execute() {
    JSEnvScope scope(nullptr);
    f_();
  }

  void OnOK() {
    JSEnvScope scope(Env());
    g_();
  }
};

template <typename F, typename G>
void JSAsync(F&& f, G&& g) {
  // NOTE(dkorolev): The bare `new` is fine here, as the object will be destructed by the `Napi` framework; I checked.
  (new JSAsyncImpl<F, G>(std::forward<F>(f), std::forward<G>(g)))->Queue();
}

}  // namespace impl
}  // namespace javascript
}  // namespace current
