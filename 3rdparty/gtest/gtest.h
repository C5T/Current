// This file added by dkorolev.

#ifndef THIRDPARTY_GTEST_H
#define THIRDPARTY_GTEST_H

#include "src/gtest-all.cc"

// From within `TEST(Module, Smoke)` returns "Smoke".
inline std::string CurrentTestName() {
  return testing::GetUnitTestImpl()->current_test_info()->name();
}

#endif  // THIRDPARTY_GTEST_H
