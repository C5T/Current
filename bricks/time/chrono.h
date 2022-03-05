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

#include "../port.h"

#include <time.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

#include "../exception.h"

#include "../util/singleton.h"
#include "../strings/fixed_size_serializer.h"
#include "../strings/util.h"

#ifdef CURRENT_WINDOWS
#define timegm _mkgmtime
#endif

namespace current {
namespace time {

#ifdef CURRENT_MOCK_TIME

// LCOV_EXCL_START
struct InconsistentSetNowException : Exception {
  InconsistentSetNowException(std::chrono::microseconds was, std::chrono::microseconds attempted)
      : Exception("SetNow() attempted to change time back to " + ToString(attempted) + ' ' + " from " + ToString(was) +
                  '.') {}
};
// LCOV_EXCL_STOP

struct MockNowImpl {
  std::chrono::microseconds mock_now_value;
  std::chrono::microseconds max_mock_now_value;
  std::mutex mutex;
};

inline const std::chrono::microseconds Now() {
  auto& impl = Singleton<MockNowImpl>();
  std::lock_guard<std::mutex> lock(impl.mutex);
  const auto now = impl.mock_now_value;
  if (impl.mock_now_value < impl.max_mock_now_value) {
    ++impl.mock_now_value;
  }
  return now;
}

inline void SetNow(std::chrono::microseconds us, std::chrono::microseconds max_us = std::chrono::microseconds(0)) {
  auto& impl = Singleton<MockNowImpl>();
  std::lock_guard<std::mutex> lock(impl.mutex);
  if (us >= impl.mock_now_value) {
    impl.mock_now_value = us;
    impl.max_mock_now_value = max_us;
  } else {
    CURRENT_THROW(InconsistentSetNowException(impl.mock_now_value, us));  // LCOV_EXCL_LINE
  }
}

inline void ResetToZero() {
  auto& impl = Singleton<MockNowImpl>();
  std::lock_guard<std::mutex> lock(impl.mutex);
  impl.mock_now_value = std::chrono::microseconds(0);
  impl.max_mock_now_value = std::chrono::microseconds(1000ll * 1000ll * 1000ll);
}

template <typename T>
void SleepUntil(T) {}

#else

// Since chrono::system_clock is not monotonic, and chrono::steady_clock is not guaranteed to be Epoch,
// use a simple wrapper around chrono::system_clock to make it strictly increasing.
struct EpochClockGuaranteeingMonotonicity {
  mutable std::atomic<int64_t> monotonic_now_us;

  EpochClockGuaranteeingMonotonicity() : monotonic_now_us(0ll) {}

  inline std::chrono::microseconds Now() const {
    int64_t now, previous_now;
    do {
      now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count();
      previous_now = monotonic_now_us.load();
      if (!(now > previous_now)) {
        now = previous_now + 1;
      }
    } while (!monotonic_now_us.compare_exchange_strong(previous_now, now));
    return std::chrono::microseconds(now);
  }
};

inline std::chrono::microseconds Now() { return Singleton<EpochClockGuaranteeingMonotonicity>().Now(); }

template <typename T>
inline void SleepUntil(T moment) {
  const auto now = Now();
  const auto desired = std::chrono::microseconds(moment);
  if (now < desired) {
    std::this_thread::sleep_for(std::chrono::microseconds(desired - now));
  }
}

#endif  // CURRENT_MOCK_TIME

}  // namespace time

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

}  // namespace strings

namespace time {

enum class TimeRepresentation { Local, UTC };

// TODO: Make it locale independent.
struct DateTimeFmts {
  // See `https://tools.ietf.org/html/rfc7231#section-7.1.1.1` for details.
  constexpr static const char* IMFFix = "%a, %d %b %Y %H:%M:%S GMT";
  constexpr static const char* RFC850 = "%A, %d-%b-%y %H:%M:%S GMT";
};

enum class SecondsToMicrosecondsPadding : bool { Lower = false, Upper = true };

struct DefaultTimeArgument {};

inline std::chrono::microseconds TimestampAsMicroseconds(DefaultTimeArgument) { return Now(); }
inline std::chrono::microseconds TimestampAsMicroseconds(std::chrono::microseconds us) { return us; }

template <typename>
struct IsTimestampImpl {
  constexpr static bool value = false;
};

template <>
struct IsTimestampImpl<std::chrono::microseconds> {
  constexpr static bool value = true;
};

template <>
struct IsTimestampImpl<DefaultTimeArgument> {
  constexpr static bool value = true;
};

template <typename T>
using IsTimestamp = IsTimestampImpl<decay_t<T>>;

template <time::TimeRepresentation T = time::TimeRepresentation::Local>
inline std::tm& FillStructTM(std::chrono::microseconds t, std::tm& output_tm) {
  std::chrono::time_point<std::chrono::system_clock> tp(t);
  time_t tt = std::chrono::system_clock::to_time_t(tp);
  ::memset(&output_tm, 0, sizeof(std::tm));
  if (T == time::TimeRepresentation::Local) {
#ifdef CURRENT_WINDOWS
    ::localtime_s(&output_tm, &tt);
#else
    ::localtime_r(&tt, &output_tm);
#endif
  } else {
#ifdef CURRENT_WINDOWS
    ::gmtime_s(&output_tm, &tt);
#else
    ::gmtime_r(&tt, &output_tm);
#endif
  }
  return output_tm;
}

template <time::TimeRepresentation T = time::TimeRepresentation::Local>
inline std::tm FillStructTM(std::chrono::microseconds t) {
  std::tm tm;
  return FillStructTM<T>(t, tm);
}

}  // namespace time

template <time::TimeRepresentation T = time::TimeRepresentation::Local>
inline std::string FormatDateTime(std::chrono::microseconds t, const char* format_string = "%Y/%m/%d %H:%M:%S") {
  std::tm tm;
  time::FillStructTM<T>(t, tm);
  char buf[1025];
  if (std::strftime(buf, sizeof(buf), format_string, &tm)) {
    return buf;
  } else {
    return ToString(t) + "us";
  }
}

inline std::string FormatDateTimeAsIMFFix(std::chrono::microseconds t) {
  return FormatDateTime<time::TimeRepresentation::UTC>(t, time::DateTimeFmts::IMFFix);
}

inline std::string FormatDateTimeAsRFC850(std::chrono::microseconds t) {
  return FormatDateTime<time::TimeRepresentation::UTC>(t, time::DateTimeFmts::RFC850);
}

template <time::TimeRepresentation T, typename STRING>
inline std::chrono::microseconds DateTimeStringToTimestamp(
    STRING&& datetime,
    const char* format_string,
    time::SecondsToMicrosecondsPadding padding = time::SecondsToMicrosecondsPadding::Lower) {
  const int64_t million = static_cast<int64_t>(1e6);
  std::tm tm;
  ::memset(&tm, 0, sizeof(std::tm));
  if (strptime(current::strings::ConstCharPtr(std::forward<STRING>(datetime)), format_string, &tm)) {
    time_t tt;
    if (T == time::TimeRepresentation::Local) {
      tm.tm_isdst = -1;
      tt = ::mktime(&tm);
    } else {
      tm.tm_isdst = 0;
      tt = ::timegm(&tm);
    }
    const auto result =
        std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::system_clock::from_time_t(tt))
            .time_since_epoch();
    if (padding == time::SecondsToMicrosecondsPadding::Lower) {
      return result;
    } else {
      return result + std::chrono::microseconds(million - 1);
    }
  } else {
    return std::chrono::microseconds(0);
  }
}

template <typename STRING>
std::chrono::microseconds UTCDateTimeStringToTimestamp(
    STRING&& datetime,
    const char* format_string,
    time::SecondsToMicrosecondsPadding padding = time::SecondsToMicrosecondsPadding::Lower) {
  return DateTimeStringToTimestamp<time::TimeRepresentation::UTC>(
      std::forward<STRING>(datetime), format_string, padding);
}

template <typename STRING>
std::chrono::microseconds LocalDateTimeStringToTimestamp(
    STRING&& datetime,
    const char* format_string,
    time::SecondsToMicrosecondsPadding padding = time::SecondsToMicrosecondsPadding::Lower) {
  return DateTimeStringToTimestamp<time::TimeRepresentation::Local>(
      std::forward<STRING>(datetime), format_string, padding);
}

template <typename STRING>
std::chrono::microseconds IMFFixDateTimeStringToTimestamp(
    STRING&& datetime, time::SecondsToMicrosecondsPadding padding = time::SecondsToMicrosecondsPadding::Lower) {
  return DateTimeStringToTimestamp<time::TimeRepresentation::UTC>(
      std::forward<STRING>(datetime), time::DateTimeFmts::IMFFix, padding);
}

template <typename STRING>
std::chrono::microseconds RFC850DateTimeStringToTimestamp(
    STRING&& datetime, time::SecondsToMicrosecondsPadding padding = time::SecondsToMicrosecondsPadding::Lower) {
  return DateTimeStringToTimestamp<time::TimeRepresentation::UTC>(
      std::forward<STRING>(datetime), time::DateTimeFmts::RFC850, padding);
}

}  // namespace current

#endif  // BRICKS_TIME_CHRONO_H
