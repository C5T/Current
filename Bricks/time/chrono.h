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

// This file can not be named `time.h`, since it would interfere with C/C++ standard header.

#ifndef BRICKS_TIME_CHRONO_H
#define BRICKS_TIME_CHRONO_H

#include <algorithm>
#include <thread>
#include <chrono>

#include "../port.h"
#include "../util/singleton.h"
#include "../strings/fixed_size_serializer.h"

namespace current {
namespace time {

#ifdef BRICKS_MOCK_TIME

struct MockNowImpl {
  std::chrono::microseconds mock_now_value;
};

inline MockNowImpl& MockNow() {
  static MockNowImpl impl;
  return impl;
}

inline const std::chrono::microseconds Now() { return MockNow().mock_now_value; }

inline void SetNow(std::chrono::microseconds us) { MockNow().mock_now_value = us; }

template <typename T>
void SleepUntil(T) {}

#else

// Since chrono::system_clock is not monotonic, and chrono::steady_clock is not guaranteed to be Epoch,
// use a simple wrapper around chrono::system_clock to make it strictly increasing.
struct EpochClockGuaranteeingMonotonicity {
  struct Impl {
    mutable uint64_t monotonic_now_us = 0ull;
    inline std::chrono::microseconds Now() const {
      const uint64_t now_us = std::chrono::duration_cast<std::chrono::microseconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count();
      monotonic_now_us = std::max(monotonic_now_us + 1, now_us);
      return std::chrono::microseconds(monotonic_now_us);
    }
  };
  static const Impl& Singleton() { return ThreadLocalSingleton<Impl>(); }
};

inline std::chrono::microseconds Now() { return EpochClockGuaranteeingMonotonicity::Singleton().Now(); }

template <typename T>
inline void SleepUntil(T moment) {
  const auto now = Now();
  const auto desired = std::chrono::microseconds(moment);
  if (now < desired) {
    std::this_thread::sleep_for(std::chrono::microseconds(desired - now));
  }
}

#endif  // BRICKS_MOCK_TIME

}  // namespace current::time

namespace strings {

template <>
struct FixedSizeSerializer<std::chrono::microseconds> {
  enum { size_in_bytes = std::numeric_limits<uint64_t>::digits10 + 1 };
  static std::string PackToString(std::chrono::microseconds x) {
    std::ostringstream os;
    os << std::setfill('0') << std::setw(size_in_bytes) << static_cast<uint64_t>(x.count());
    return os.str();
  }
  static std::chrono::microseconds UnpackFromString(std::string const& s) {
    uint64_t x;
    std::istringstream is(s);
    is >> x;
    return std::chrono::microseconds(x);
  }
};

}  // namespace current::strings

}  // namespace current

#endif  // BRICKS_TIME_CHRONO_H
