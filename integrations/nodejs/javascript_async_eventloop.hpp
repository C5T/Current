#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

// This header file implements `JSAsyncEventLoopImpl`, a poor man's "full event loop" implementation that is based on
// the very `JSAsync` / `AsyncWorker`. The implementation is suboptimal because it spawns a new `AsyncWorker` on every
// new in-and-out call from within and outside the JavaScript event loop.
//
// The imprortant thing is that the semantics is right: the very thread-local singleton for JS environment is respected,
// and the user code reads as it should.
//
// NOTE(dkorolev): I expect to get back to this code once performance considerations become important.

#include "javascript_async.hpp"
#include "javascript_function_cont.hpp"  // For `LambdaSignatureExtractor`.

namespace current {
namespace javascript {
namespace impl {

class JSAsyncEventLoopImpl final {
 private:
  mutable std::mutex mutex_;
  mutable std::condition_variable cv_;

  std::function<bool()> next_function_;
  bool last_call_result_ = true;
  bool terminated_ = false;

  std::thread thread_;

 public:
  ~JSAsyncEventLoopImpl() { thread_.join(); }

  bool InternalSyncRun(std::function<bool()> next_function) {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      // NOTE(dkorolev): This would eventually need to be a queue.
      if (next_function_) {
        Napi::Error::Fatal("CurrentJSBinding", "There's a `next_function_` in call to `InternalSyncRun()`.");
      }
      next_function_ = next_function;
    }
    cv_.notify_one();
    {
      // NOTE(dkorolev): This is not thread-safe with respect to collecting the result of the call,
      // in the case there is more than one thread trying to make calls.
      // TODO(dkorolev): Make this a proper `queue` of C++ promises.
      std::unique_lock<std::mutex> lock(mutex_);
      cv_.wait(lock, [this]() { return !next_function_; });
    }
    return last_call_result_;
  }

  void InternalSpawnThread(std::function<void()> f) {
    thread_ = std::thread([this, f]() {
      f();
      if (!terminated_) {
        // NOTE(dkorolev): This is safe, as in the "worst" case concurrent scenario, the function that currently is
        // running would return `false`, and thus this "second" function that returns `false` would just be left
        // "unexecuted", as the instance destroys itself. The termination job would be done correctly regardless.
        InternalSyncRun([]() { return false; });
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return terminated_; });
      }
    });
  }

  void InternalWait() const {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (next_function_) {
        break;
      }
      cv_.wait(lock, [this]() { return next_function_; });
    }
  }

  static void InternalWork(std::shared_ptr<JSAsyncEventLoopImpl> self) {
    {
      std::lock_guard<std::mutex> lock(self->mutex_);
      if (!self->next_function_) {
        Napi::Error::Fatal("CurrentJSBinding", "No next_function_ function to call in `InternalWork()`.");
      }
      // NOTE(dkorolev): In a truly thread-safe code this would set a promise instead.
      self->last_call_result_ = self->next_function_();
      self->next_function_ = nullptr;
      if (!self->last_call_result_) {
        self->terminated_ = true;
      }
    }
    if (self->last_call_result_) {
      self->cv_.notify_one();
      JSAsync([self]() { self->InternalWait(); }, [self]() { InternalWork(self); });
    }
  }
};

struct JSAsyncEventLoopRunnerImpl final {
  JSAsyncEventLoopImpl* worker_;
  explicit JSAsyncEventLoopRunnerImpl(JSAsyncEventLoopImpl* worker) : worker_(worker) {}

  template <class F, class FS = typename LambdaSignatureExtractor<std::decay_t<F>>::std_function_t>
  typename std::enable_if<std::is_same<FS, std::function<bool()>>::value, bool>::type operator()(F f) const {
    return worker_->InternalSyncRun(f);
  }

  template <class F, class FS = typename LambdaSignatureExtractor<std::decay_t<F>>::std_function_t>
  typename std::enable_if<std::is_same<FS, std::function<void()>>::value>::type operator()(F f) const {
    worker_->InternalSyncRun([f]() {
      f();
      return true;
    });
  }
};

using JSAsyncEventLoopRunner = const JSAsyncEventLoopRunnerImpl&;

inline void JSAsyncEventLoop(std::function<void(JSAsyncEventLoopRunner)> f) {
  auto worker = std::make_shared<JSAsyncEventLoopImpl>();
  JSAsync([worker]() { worker->InternalWait(); }, [worker]() { JSAsyncEventLoopImpl::InternalWork(worker); });
  worker->InternalSpawnThread([worker, f]() {
    JSAsyncEventLoopRunnerImpl impl(worker.get());
    f(impl);
  });
}

}  // namespace impl
}  // namespace javascript
}  // namespace current
