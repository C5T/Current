/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_UTIL_FUTURE_H
#define BRICKS_UTIL_FUTURE_H

#include "../port.h"

#include <iostream>
#include <cstdlib>
#include <future>

namespace current {

// `current::Future<>` can be declared `Strict`, in which case
// not explicitly waiting for its result is a runtime error.
// The default, `Forgiving`, mode does not have this requirement.
enum class StrictFuture : bool { Forgiving = false, Strict = true };

template <typename T, StrictFuture T_STRICT = StrictFuture::Forgiving>
struct FutureImpl {
  FutureImpl() = delete;
  FutureImpl(std::future<T>&& rhs) : f_(std::move(rhs)), used_(false) {}
  FutureImpl(FutureImpl<T, StrictFuture::Forgiving>&& rhs) : f_(std::move(rhs.f_)), used_(false) {}
  FutureImpl(FutureImpl<T, StrictFuture::Strict>&& rhs) : f_(std::move(rhs.f_)), used_(false) {
    rhs.used_ = true;
  }
  ~FutureImpl() {
    if (T_STRICT == StrictFuture::Strict && !used_) {
      std::cerr << "Strict future has been left hanging, while `.Go()` or `.Wait()` must have been called."
                << std::endl;
      std::exit(-1);
    }
  }

  T Go() {
    used_ = true;
    return std::forward<T>(f_.get());
  }
  void Wait() {
    used_ = true;
    f_.wait();
  }

 private:
  std::future<T> f_;
  bool used_ = false;
};

template <StrictFuture T_STRICT>
struct FutureImpl<void, T_STRICT> {
  FutureImpl() = delete;
  FutureImpl(std::future<void>&& rhs) : f_(std::move(rhs)), used_(false) {}
  FutureImpl(FutureImpl<void, StrictFuture::Forgiving>&& rhs) : f_(std::move(rhs.f_)), used_(false) {}
  FutureImpl(FutureImpl<void, StrictFuture::Strict>&& rhs) : f_(std::move(rhs.f_)), used_(false) {
    rhs.used_ = true;
  }

  void Go() {
    used_ = true;
    f_.wait();
  }
  void Wait() {
    used_ = true;
    f_.wait();
  }

 private:
  std::future<void> f_;
  bool used_ = false;
};

// To fight "error: default template arguments may not be used in partial specializations" for `void`.
template <typename T, StrictFuture T_STRICT = StrictFuture::Forgiving>
using Future = FutureImpl<T, T_STRICT>;

}  // namespace current

#endif  // BRICKS_UTIL_FUTURE_H
