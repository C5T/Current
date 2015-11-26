// This file added by dkorolev.
// "dflags.h" should already be included for this header to do its job.
//
// NOTE: The header guard is the same for gtest-main.h and gtest-main-with-dflags.h
//       This is to allow compiling multiple *.cc tests as a single binary by concatenating them,
//       while some might use gtest-main.h and others might use gtest-main-with-dflags.h.

#ifndef THIRDPARTY_GTEST_MAIN_H
#define THIRDPARTY_GTEST_MAIN_H

#define _WINSOCKAPI_  // `gtest` includes `windows.h` on Windows, and this macro has to be defined before it.

#include "gtest.h"

DEFINE_string(current_runtime_arch, "", "The expected architecture to run on, `uname` on *nix systems.");

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  ParseDFlags(&argc, &argv);
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

#endif  // THIRDPARTY_GTEST_MAIN_WITH_DFLAGS_H
