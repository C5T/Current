/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_UTIL_WAITABLE_TERMINATE_SIGNAL_H
#define BRICKS_UTIL_WAITABLE_TERMINATE_SIGNAL_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <unordered_set>

namespace current {

// An instance of `WaitableTerminateSignal` is the logic allowing to send another thread a signal to tear down,
// while making it possible for that thread to wait for some external event.
// A common usecase is a thread listening to events, that has exhausted its buffered entries and is presently
// waiting for the new ones to arrive. It should still be possible to terminate externally, hence the wait for
// new entries should not be a simple wait on a conditional variable. `WaitableTerminateSignal` enables this.
class WaitableTerminateSignal {
 public:
  explicit WaitableTerminateSignal() noexcept : stop_signal_(false) {}

  // Can always check whether it is time to terminate. Thread-safe.
  operator bool() const noexcept { return stop_signal_; }

  // Sends the termination signal. Thread-safe.
  void SignalExternalTermination() noexcept {
    stop_signal_ = true;
    condition_variable_.notify_all();
  }

  // To be called by external users that the thread using this `WaitableTerminateSignal` could wait upon.
  // Thread-safe.
  void NotifyOfExternalWaitableEvent() noexcept { condition_variable_.notify_all(); }

  // Waits until the provided method returns `true`, or until `SignalExternalTermination()` has been called.
  template <typename F>
  bool WaitUntil(std::unique_lock<std::mutex>& lock, F&& external_condition) noexcept {
    bool wait_done;
    const auto stop_condition = [this, &external_condition, &wait_done]() {
      wait_done = stop_signal_ || external_condition();
      return wait_done;
    };

    do {
      // TODO(dkorolev): Fix this deadlock workaround. Should be enough with `wait()`, not `wait_for()`.
      condition_variable_.wait_for(lock, std::chrono::milliseconds(25), stop_condition);
    } while (!wait_done);

    return stop_signal_;
  }

 private:
  WaitableTerminateSignal(const WaitableTerminateSignal&) = delete;

  std::atomic_bool stop_signal_;
  std::condition_variable condition_variable_;
};

// Enables subscribing multiple `WaitableTerminateSignal`-s to be notified of new events at once.
class WaitableTerminateSignalBulkNotifier {
 public:
  // THREAD-SAFE.
  class Scope {
   public:
    Scope(WaitableTerminateSignalBulkNotifier& bulk, WaitableTerminateSignal& signal) noexcept
        : bulk_(bulk), notifier_(signal) {
      bulk_.RegisterPendingNotifier(notifier_);
    }
    Scope(WaitableTerminateSignalBulkNotifier* bulk, WaitableTerminateSignal& signal) noexcept
        : bulk_(*bulk), notifier_(signal) {
      bulk_.RegisterPendingNotifier(notifier_);
    }
    ~Scope() { bulk_.UnRegisterPendingNotifier(notifier_); }

   private:
    Scope() = delete;

    WaitableTerminateSignalBulkNotifier& bulk_;
    WaitableTerminateSignal& notifier_;
  };

  // THREAD-SAFE.
  void NotifyAllOfExternalWaitableEvent() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (WaitableTerminateSignal* signal : active_signals_) {
      signal->NotifyOfExternalWaitableEvent();
    }
  }

  // THREAD-SAFE.
  void RegisterPendingNotifier(WaitableTerminateSignal& signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_signals_.insert(&signal);
  }

  // THREAD-SAFE.
  void UnRegisterPendingNotifier(WaitableTerminateSignal& signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    active_signals_.erase(&signal);
  }

 private:
  // Can't use `reference_wrapper` w/o a global `operator<()` -- a member one doesn't nail it. -- D.K.
  std::mutex mutex_;
  std::unordered_set<WaitableTerminateSignal*> active_signals_;
};

}  // namespace current

#endif  // BRICKS_UTIL_WAITABLE_TERMINATE_SIGNAL_H
