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
  const int64_t dt = static_cast<int64_t>(b - a);
#if !defined(CURRENT_WINDOWS) && !defined(CURRENT_APPLE)
  const int64_t allowed_skew = 3000;
#else
  const int64_t allowed_skew = 25000;  // Some systems are slower in regard to this test.
#endif
  EXPECT_GE(dt, 50000 - allowed_skew);
  EXPECT_LE(dt, 50000 + allowed_skew);
}

#else

// Emit a warning that the test is disabled.
// Sadly, `#warning` is non-standard.

#ifndef _MSC_VER

// `g++` and `clang++` style.
#warning "==================================================================="
#warning "Ignore this warning if a full batch test is being run!"
#warning "A flaky test comparing against wall time is disabled for batch run."
#warning "==================================================================="

#else

#pragma message("===================================================================")
#pragma message("Ignore this warning if a full batch test is being run!")
#pragma message("A flaky test comparing against wall time is disabled for batch run.")
#pragma message("===================================================================")

#endif  // _MSC_VER
#endif  // CURRENT_MOCK_TIME
