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

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <unordered_set>

namespace bricks {

// An instance of `WaitableTerminateSignal` is the logic allowing to send another thread a signal to tear down,
// while making it possible for that thread to wait for some external event.
// A common usecase is a thread listening to events, that has exhausted its buffered entries and is presently
// waiting for the new ones to arrive. It should still be possible to terminate externally, hence the wait for
// new entries should not be a simple wait on a conditional variable. `WaitableTerminateSignal` enables this.
class WaitableTerminateSignal {
 public:
  explicit WaitableTerminateSignal() {}

  // Can always check whether it is time to terminate. Thread-safe.
  operator bool() const { return stop_signal_; }

  // Sends the termination signal. Thread-safe.
  void SignalExternalTermination() {
    // Important to do `notify_all()` from within a locked session guarding
    // the external, client-supplied, mutex that has been passed
    // into `WaitUntil()` in case one is active now.
    std::lock_guard<std::mutex> guard(client_mutex_mutex_);
    if (client_mutex_) {
      std::lock_guard<std::mutex> client_mutex_guard(*client_mutex_);
      stop_signal_ = true;
      condition_variable_.notify_all();
    } else {
      stop_signal_ = true;
      condition_variable_.notify_all();
    }
  }

  // To be called by external users that the thread using this `WaitableTerminateSignal` could wait upon.
  // Thread-safe.
  void NotifyOfExternalWaitableEvent() { condition_variable_.notify_all(); }

  // Waits until the provided method returns `true`, or until `SignalExternalTermination()` has been called.
  template <typename F>
  bool WaitUntil(std::unique_lock<std::mutex>& lock, F&& external_condition) {
    assert(lock.mutex());

    {
      std::lock_guard<std::mutex> guard(client_mutex_mutex_);
      assert(!client_mutex_);
      client_mutex_ = lock.mutex();
    }

    // TODO(dkorolev): Remove the `while` once we confirm that "spurious awakenings" are not the problem here.
    // TODO(dkorolev): Here and in at least one more place in `Current` I remember of.
    // Appears safe to me to remove (http://en.cppreference.com/w/cpp/thread/condition_variable/wait),
    // just want to double-check on several architectures before doing so.
    const auto stop_condition = [this, &external_condition]() { return stop_signal_ || external_condition(); };
    while (!stop_condition()) {
      condition_variable_.wait(lock, stop_condition);
    }

    {
      std::lock_guard<std::mutex> guard(client_mutex_mutex_);
      assert(client_mutex_ == lock.mutex());
      client_mutex_ = nullptr;
    }

    return stop_signal_;
  }

 private:
  WaitableTerminateSignal(const WaitableTerminateSignal&) = delete;

  // To make sure `WaitUntil()` does not intersect with `SignalExternalTermination()`,
  // keep a pointer to the top-level `std::mutex` used inside the `std::unique_lock`,
  // which the client has been passed down to `WaitUntil()`.
  std::mutex client_mutex_mutex_;
  std::mutex* client_mutex_ = nullptr;

  volatile bool stop_signal_ = false;
  std::condition_variable condition_variable_;
};

}  // namespace bricks

#endif  // BRICKS_UTIL_WAITABLE_TERMINATE_SIGNAL_H
