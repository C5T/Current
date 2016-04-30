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

// NOTE: The header guard is the same for gtest-main.h and gtest-main-with-dflags.h
//       This is to allow compiling multiple *.cc tests as a single binary by concatenating them,
//       while some might use gtest-main.h and others might use gtest-main-with-dflags.h.

#ifndef THIRDPARTY_GTEST_MAIN_H
#define THIRDPARTY_GTEST_MAIN_H

#define _WINSOCKAPI_  // `gtest` includes `windows.h` on Windows, and this macro has to be defined before it.

#include "gtest.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // Postpone the `Death tests use fork(), which is unsafe particularly in a threaded context.` warning.
  // Via https://code.google.com/p/googletest/wiki/AdvancedGuide#Death_Test_Styles
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  if (::testing::FLAGS_gtest_repeat != 1) {
    // Break on any error when `--gtest_repeat` is set.
    // Otherwise a failure in one of hundreds or thousands of runs may get unnoticed.
    // This condition passes both for number of tests, ex. 100, and for `-1`, for "run indefinitely".
    // Added by Dima. -- @dkorolev.
    ::testing::FLAGS_gtest_break_on_failure = true;
  }
  const auto result = RUN_ALL_TESTS();
#ifdef _WIN32
  // It's easier for the developers to just press Enter after the tests are done compared to
  // configuring Visual Studio to not close the application terminal by default.
  {
    std::string s;
    std::cout << std::endl << "Done executing, press Enter to terminate.";
    std::getline(std::cin, s);
  }
#endif
  return result;
}

#endif  // THIRDPARTY_GTEST_MAIN_H
