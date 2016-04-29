/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef THIRDPARTY_GTEST_H
#define THIRDPARTY_GTEST_H

#include "src/gtest-all.cc"

// From within `TEST(Module, Smoke)` returns "Smoke".
inline std::string CurrentTestName() {
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
