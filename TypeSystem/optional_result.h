/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_OPTIONAL_RESULT_H
#define CURRENT_TYPE_SYSTEM_OPTIONAL_RESULT_H

#include "exceptions.h"

namespace current {

struct OptionalResultExists {};
struct OptionalResultMissing {};

template <typename T>
class OptionalResult {
 public:
  OptionalResult(const OptionalResultMissing&) : exists_(false) {}

  OptionalResult(OptionalResult&& rhs) : value_(std::move(rhs.value_)), exists_(rhs.exists_) {}

  OptionalResult& operator=(OptionalResult&& rhs) {
    value_ = std::move(rhs.value_);
    exists_ = rhs.exists_;
    return *this;
  }

  OptionalResult(T&& result) : value_(std::move(result)), exists_(true) {}

  OptionalResult& operator=(T&& result) {
    value_ = std::move(result);
    exists_ = true;
    return *this;
  }

  OptionalResult() = delete;
  OptionalResult(const OptionalResult&) = delete;
  OptionalResult& operator=(const OptionalResult&) = delete;

  bool ExistsImpl() const { return exists_; }

  T&& ValueImpl() {
    if (exists_) {
      return std::move(value_);
    } else {
      throw NoValueOfTypeException<T>();
    }
  }

  const T& ValueImpl() const {
    if (exists_) {
      return value_;
    } else {
      throw NoValueOfTypeException<T>();  // LCOV_EXCL_LINE
    }
  }

 private:
  T value_;
  bool exists_;
};

template <>
class OptionalResult<void> {
 public:
  explicit OptionalResult(const OptionalResultExists&) : exists_(true) {}
  // TODO(dkorolev) + TODO(mzhurovich): Test this.
  explicit OptionalResult(const OptionalResultMissing&) : exists_(false) {}

  OptionalResult(OptionalResult&& rhs) : exists_(rhs.exists_) {}

  OptionalResult& operator=(OptionalResult&& rhs) {
    exists_ = rhs.exists_;
    return *this;
  }

#ifndef _MSC_VER
  OptionalResult() = delete;
  OptionalResult(const OptionalResult&) = delete;
  OptionalResult& operator=(const OptionalResult&) = delete;
#else
  OptionalResult() = default;
  OptionalResult(const OptionalResult&) = default;
  OptionalResult& operator=(const OptionalResult&) = default;
#endif

  bool ExistsImpl() const { return exists_; }

 private:
  bool exists_;
};

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_OPTIONAL_RESULT_H
