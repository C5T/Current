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

#ifndef BRICKS_UTIL_HTTP_SCOPED_REGISTERER_H
#define BRICKS_UTIL_HTTP_SCOPED_REGISTERER_H

#include "../../port.h"

#include <set>
#include <functional>

namespace current {

template <class DIFFERENTIATOR>
class AccumulativeScopedDeleter {
 public:
  // Construction by move only, no copy.
  AccumulativeScopedDeleter() = default;
  AccumulativeScopedDeleter(std::function<void()> f) : captured_{f} {}

  AccumulativeScopedDeleter(AccumulativeScopedDeleter&& rhs) : captured_(std::move(rhs.captured_)) {
    rhs.captured_.clear();
  }
  AccumulativeScopedDeleter& operator=(AccumulativeScopedDeleter&& rhs) {
    ReleaseCaptured();
    captured_.swap(rhs.captured_);
    return *this;
  }

  AccumulativeScopedDeleter(const AccumulativeScopedDeleter&) = delete;
  AccumulativeScopedDeleter& operator=(const AccumulativeScopedDeleter&) = delete;

  // Destruction unregisters previously added instances.
  ~AccumulativeScopedDeleter() { ReleaseCaptured(); }

  // Add a instance, or a set of instances, erasing them from the `rhs`.
  AccumulativeScopedDeleter& operator+=(AccumulativeScopedDeleter&& rhs) {
    for (const auto& instance : rhs.captured_) {
      captured_.push_back(instance);
    }
    rhs.captured_.clear();
    return *this;
  }

  // Return a new object containing both current and `rhs` instances, clean up `this`.
  // Used to intialize `AccumulativeScopedDeleter` using initializer list, with multiple instances.
  AccumulativeScopedDeleter operator+(AccumulativeScopedDeleter&& rhs) {
    operator+=(std::move(rhs));
    return AccumulativeScopedDeleter(std::move(*this));
  }

 private:
  void ReleaseCaptured() {
    // Release in LIFO order.
    for (auto rit = captured_.rbegin(); rit != captured_.rend(); ++rit) {
      (*rit)();
    }
    captured_.clear();
  }

  std::vector<std::function<void()>> captured_;
};

}  // namespace current

#endif  // BRICKS_UTIL_HTTP_SCOPED_REGISTERER_H
