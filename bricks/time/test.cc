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

#include <thread>
#include <chrono>

#include "chrono.h"

#include "../../3rdparty/gtest/gtest-main.h"

#ifndef CURRENT_MOCK_TIME

// This smoke test is flaky, but it does the job of comparing current::time::Now() to wall time.
TEST(Time, SmokeTest) {
  const std::chrono::microseconds a = current::time::Now();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const std::chrono::microseconds b = current::time::Now();
  const int64_t dt = static_cast<int64_t>((b - a).count());
#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE) && !defined(CURRENT_CI)
  const int64_t allowed_skew = 3000;
#else
  const int64_t allowed_skew = 250000;  // NOTE(dkorolev): As of Feb 2021 Travis can be veeery slow.
#endif
  EXPECT_GE(dt, 50000 - allowed_skew);
  EXPECT_LE(dt, 50000 + allowed_skew);
}

#else

#ifndef CURRENT_COVERAGE_REPORT_MODE

// Emit a warning that the test is disabled.
// Sadly, `#warning` is non-standard.

#ifndef _MSC_VER

// `g++` and `clang++` style.
#warning "==================================================================="
#warning "Ignore this warning if a full batch test is being run!"
#warning "A flaky test comparing against wall time is disabled for batch run."
#warning "==================================================================="

#else

// Microsoft style.
#pragma message("===================================================================")
#pragma message("Ignore this warning if a full batch test is being run!")
#pragma message("A flaky test comparing against wall time is disabled for batch run.")
#pragma message("===================================================================")

#endif  // CURRENT_WINDOWS

#endif  // CURRENT_COVERAGE_REPORT_MODE

#endif  // CURRENT_MOCK_TIME

static_assert(current::time::IsTimestamp<std::chrono::microseconds>::value, "");
static_assert(current::time::IsTimestamp<current::time::DefaultTimeArgument>::value, "");

static_assert(!current::time::IsTimestamp<std::chrono::hours>::value, "");
static_assert(!current::time::IsTimestamp<uint64_t>::value, "");
static_assert(!current::time::IsTimestamp<size_t>::value, "");

TEST(Time, DateTimeFormatFunctions) {
  // Smoke test.
  ASSERT_NE(current::IMFFixDateTimeStringToTimestamp("Sun, 24 Apr 2016 01:31:01 GMT").count(),
            current::IMFFixDateTimeStringToTimestamp("Sun, 24 Apr 2016 01:31:01 PDT").count());

  // Real test.
  const auto t = std::chrono::microseconds(1461461461461461);
  EXPECT_EQ("2016/04/24 01:31:01", current::FormatDateTime<current::time::TimeRepresentation::UTC>(t));
  EXPECT_EQ("Sun, 24 Apr 2016 01:31:01 GMT", current::FormatDateTimeAsIMFFix(t));
  EXPECT_EQ("Sunday, 24-Apr-16 01:31:01 GMT", current::FormatDateTimeAsRFC850(t));
  EXPECT_EQ(1461461461000000, current::IMFFixDateTimeStringToTimestamp("Sun, 24 Apr 2016 01:31:01 GMT").count());
  EXPECT_EQ(1461461461999999,
            current::IMFFixDateTimeStringToTimestamp("Sun, 24 Apr 2016 01:31:01 GMT",
                                                     current::time::SecondsToMicrosecondsPadding::Upper)
                .count());
  EXPECT_EQ(0, current::IMFFixDateTimeStringToTimestamp("Not valid string,").count());
  EXPECT_EQ(
      0,
      current::IMFFixDateTimeStringToTimestamp("Not valid string,", current::time::SecondsToMicrosecondsPadding::Upper)
          .count());
  EXPECT_EQ(1461461461000000, current::RFC850DateTimeStringToTimestamp("Sunday, 24-Apr-16 01:31:01 GMT").count());
  EXPECT_EQ(1461461461999999,
            current::RFC850DateTimeStringToTimestamp("Sunday, 24-Apr-16 01:31:01 GMT",
                                                     current::time::SecondsToMicrosecondsPadding::Upper)
                .count());
  EXPECT_EQ(0, current::RFC850DateTimeStringToTimestamp("Not valid string,").count());
}
