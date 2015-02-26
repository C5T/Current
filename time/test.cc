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

#include "../3party/gtest/gtest-main.h"

// This smoke test is flaky, but it does the job of comparing bricks::time::Now() to wall time.
TEST(Time, SmokeTest) {
  const bricks::time::EPOCH_MILLISECONDS a = bricks::time::Now();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  const bricks::time::EPOCH_MILLISECONDS b = bricks::time::Now();
  const int64_t dt = static_cast<int64_t>(b - a);
#ifndef BRICKS_WINDOWS
  const int64_t allowed_skew = 3;
#else
  const int64_t allowed_skew = 25;  // Visual Studio is slower in regard to this test.
#endif
  EXPECT_GE(dt, 50 - allowed_skew);
  EXPECT_LE(dt, 50 + allowed_skew);
}
