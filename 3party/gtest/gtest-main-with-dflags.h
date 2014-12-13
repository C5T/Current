// This file added by dkorolev.

#ifndef THIRDPARTY_GTEST_MAIN_WITH_DFLAGS_H
#define THIRDPARTY_GTEST_MAIN_WITH_DFLAGS_H

#include "gtest.h"

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  testing::InitGoogleTest(&argc, argv);
  // Postpone the `Death tests use fork(), which is unsafe particularly in a threaded context.` warning.
  // Via https://code.google.com/p/googletest/wiki/AdvancedGuide#Death_Test_Styles
  ::testing::FLAGS_gtest_death_test_style = "threadsafe";
  return RUN_ALL_TESTS();
}

#endif  // THIRDPARTY_GTEST_MAIN_WITH_DFLAGS_H
