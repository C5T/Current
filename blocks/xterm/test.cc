/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2021 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "progress.h"

#include <thread>

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#ifndef CURRENT_CI
DEFINE_double(progress_line_delay, 0.5, "The delay, in seconds, between progress updates.");
#else
DEFINE_double(progress_line_delay, 0, "The delay, in seconds, between progress updates.");
#endif  // CURRENT_CI

TEST(ProgressLine, GetUndecoratedString) {
  current::ProgressLine progress;
  using namespace current::vt100;
  EXPECT_EQ("", progress.GetUndecoratedString());
  progress << bold << green << "[----------]" << reset << " Testing ...";
  EXPECT_EQ("[----------] Testing ...", progress.GetUndecoratedString());
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(1e3 * FLAGS_progress_line_delay)));
  progress << bold << green << "[----------]" << reset << ' ' << italic << red << "CO" << green << "LO" << blue << "RS";
  EXPECT_EQ("[----------] COLORS", progress.GetUndecoratedString());
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(1e3 * FLAGS_progress_line_delay)));
  progress << bold << green << "[----------]" << reset << bold << " OK!";
  EXPECT_EQ("[----------] OK!", progress.GetUndecoratedString());
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int64_t>(1e3 * FLAGS_progress_line_delay)));
}
