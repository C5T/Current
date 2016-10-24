/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev, <dmitry.korolev@gmail.com>.

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

#ifndef BRICKS_UTIL_ACCUMULATIVE_SCOPED_DELETER_H
#define BRICKS_UTIL_ACCUMULATIVE_SCOPED_DELETER_H

#include "../../port.h"

#include <functional>
#include <vector>

namespace current {

// TODO(dkorolev): Later.
// enum class AccumulativeScopedDeleterPolicy { GarbageCollect = 0, LeaveHanging = 1, AssertNotLeftHanging = 2
// };
// template <class DIFFERENTIATOR, AccumulativeScopedDeleterPolicy POLICY =
// AccumulativeScopedDeleterPolicy::GarbageCollect>
template <class DIFFERENTIATOR, bool SHOULD_DELETE = true>
class AccumulativeScopedDeleter {
 public:
  // Construction by move only, no copy.
  AccumulativeScopedDeleter() = default;
  AccumulativeScopedDeleter(std::function<void()> f) : captured_{f} {}

  template <bool B>
  AccumulativeScopedDeleter(AccumulativeScopedDeleter<DIFFERENTIATOR, B>&& rhs) : captured_(std::move(rhs.captured_)) {
    rhs.captured_.clear();
  }
  template <bool B>
  AccumulativeScopedDeleter& operator=(AccumulativeScopedDeleter<DIFFERENTIATOR, B>&& rhs) {
    ReleaseCaptured();
    captured_.swap(rhs.captured_);
    return *this;
  }

  AccumulativeScopedDeleter(const AccumulativeScopedDeleter&) = delete;
  AccumulativeScopedDeleter& operator=(const AccumulativeScopedDeleter&) = delete;

  // Destruction unregisters previously added instances.
  ~AccumulativeScopedDeleter() { ReleaseCaptured(); }

  // Also enable assigning `nullptr` explicitly.
  void operator=(std::nullptr_t) { ReleaseCaptured(); }

  // Adds an instance, or several instances, erasing them from the `rhs`.
  template <bool B>
  AccumulativeScopedDeleter& operator+=(AccumulativeScopedDeleter<DIFFERENTIATOR, B>&& rhs) {
    for (const auto& instance : rhs.captured_) {
      captured_.emplace_back(std::move(instance));
    }
    rhs.captured_.clear();
    return *this;
  }

  // Returns a new object containing both current and `rhs` instances, cleans up `this`.
  // Used to intialize `AccumulativeScopedDeleter` with multiple instances, in an initializer list.
  template <bool B>
  AccumulativeScopedDeleter<DIFFERENTIATOR, SHOULD_DELETE || B> operator+(
      AccumulativeScopedDeleter<DIFFERENTIATOR, B>&& rhs) {
    AccumulativeScopedDeleter<DIFFERENTIATOR, SHOULD_DELETE || B> result;
    result += std::move(*this);
    result += std::move(rhs);
    return result;
  }

 private:
  void ReleaseCaptured() {
    if (SHOULD_DELETE) {
      // Release in LIFO order.
      for (auto rit = captured_.rbegin(); rit != captured_.rend(); ++rit) {
        (*rit)();
      }
    }
    captured_.clear();
  }

  friend class AccumulativeScopedDeleter<DIFFERENTIATOR, !SHOULD_DELETE>;
  std::vector<std::function<void()>> captured_;
};

}  // namespace current

#endif  // BRICKS_UTIL_ACCUMULATIVE_SCOPED_DELETER_H
