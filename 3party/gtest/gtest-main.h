// This file added by dkorolev.
// NOTE: The header guard is the same for gtest-main.h and gtest-main-with-dflags.h
//       This is to allow compiling multiple *.cc tests as a single binary by concatenating them,
//       while some might use gtest-main.h and others might use gtest-main-with-dflags.h.

#ifndef THIRDPARTY_GTEST_MAIN_H
#define THIRDPARTY_GTEST_MAIN_H

#include "gtest.h"

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  // Postpone the `Death tests use fork(), which is unsafe particularly in a threaded context.` warning.
  // Via https://code.google.com/p/googletest/wiki/AdvancedGuide#Death_Test_Styles
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
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
