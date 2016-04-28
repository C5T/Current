// This file added by dkorolev.

#ifndef THIRDPARTY_GTEST_H
#define THIRDPARTY_GTEST_H

#include "src/gtest-all.cc"

// From within `TEST(Module, Smoke)` returns "Smoke".
static std::string CurrentTestName() {
  return testing::GetUnitTestImpl()->current_test_info()->name();
}

// Effectively returns `argv[0]`.
inline std::string CurrentBinaryRelativePathAndName() {
  return testing::internal::g_executable_path;
}

// Returns path part of `argv[0]`.
inline std::string CurrentBinaryRelativePath() {
  using namespace testing::internal;
  return FilePath(g_executable_path).RemoveFileName().RemoveTrailingPathSeparator().string();
}

// Returns current working directory.
inline std::string CurrentDir() {
  return testing::internal::FilePath::GetCurrentDir().string();
}

// Returns "working directory + path_separator + binary relative path".
inline std::string CurrentBinaryFullPath() {
  using namespace testing::internal;
  return FilePath::ConcatPaths(FilePath::GetCurrentDir(), FilePath(CurrentBinaryRelativePath())).string();
}

#endif  // THIRDPARTY_GTEST_H
