/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#define BRICKS_RANDOM_FIX_SEED

#include "syscalls.h"

#include "../dflags/dflags.h"
#include "../file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

#include <thread>

DEFINE_string(current_base_dir_for_dlopen_test,
              "",
              "If set, the top-level `current/` dir for `dlopen`-based integrations to symlink to.");

#ifndef CURRENT_WINDOWS
TEST(Syscalls, PipedOutputSingleLine) {
  EXPECT_EQ("Hello, World!", current::bricks::system::SystemCallReadPipe("echo 'Hello, World!'").ReadLine());
}

TEST(Syscalls, PipedOutputMultipleLines) {
  current::bricks::system::SystemCallReadPipe pipe("echo Hello; echo World");
  EXPECT_TRUE(pipe);
  EXPECT_EQ("Hello", pipe.ReadLine());
  EXPECT_TRUE(pipe);
  EXPECT_EQ("World", pipe.ReadLine());
  // Uncertain whether it should be EOF here or no, but it sure will be after the next, "empty", line is read.
  EXPECT_EQ("", pipe.ReadLine());
  EXPECT_FALSE(pipe);
}
#endif  // CURRENT_WINDOWS

#if 0
TEST(Syscalls, PopenException) {
  ASSERT_THROW(current::bricks::system::SystemCallReadPipe("/does/not/exist"),
               current::bricks::system::PopenCallException);
}
#endif

TEST(Syscalls, SystemCall) {
  const std::string tmp1_file_name = current::FileSystem::GenTmpFileName();
  const std::string tmp2_file_name = current::FileSystem::GenTmpFileName();
  const auto tmp1_deleter = current::FileSystem::ScopedRmFile(tmp1_file_name);
  const auto tmp2_deleter = current::FileSystem::ScopedRmFile(tmp2_file_name);
  current::FileSystem::WriteStringToFile("OK", tmp1_file_name.c_str());
  current::FileSystem::WriteStringToFile("NO", tmp2_file_name.c_str());
  EXPECT_EQ("NO", current::FileSystem::ReadFileAsString(tmp2_file_name));
  current::bricks::system::SystemCall(std::string("cp ") + tmp1_file_name + ' ' + tmp2_file_name);
  EXPECT_EQ("OK", current::FileSystem::ReadFileAsString(tmp2_file_name));
}

#ifndef CURRENT_WINDOWS
TEST(Syscalls, DLOpen) {
  current::bricks::system::JITCompiledCPP lib("int foo() { static int c = 42; return c++; }");
  auto foo1 = lib.template Get<int (*)()>("_Z3foov");  // The C++-mangled name for `foo`, use `objdump -t` if unsure.
  EXPECT_EQ(42, foo1());
  EXPECT_EQ(43, foo1());
  auto dynamic_lib = current::bricks::system::DynamicLibrary(lib.GetSOName());
  auto foo2 = dynamic_lib.template Get<int (*)()>("_Z3foov");
  EXPECT_EQ(44, foo2());
  EXPECT_EQ(45, foo1());
}

TEST(Syscalls, DLOpenExternC) {
  current::bricks::system::JITCompiledCPP lib("extern \"C\" int bar() { static int c = 100; return c++; }");
  auto bar1 = lib.template Get<int (*)()>("bar");  // With `extern "C"` there's no mangling.
  EXPECT_EQ(100, bar1());
  auto dynamic_lib = current::bricks::system::DynamicLibrary(lib.GetSOName());
  auto bar2 = dynamic_lib.template Get<int (*)()>("bar");
  EXPECT_EQ(101, bar2());
  EXPECT_EQ(102, bar1());
  EXPECT_EQ(103, bar2());
}

TEST(Syscalls, DLOpenHasCurrentHeader) {
  current::bricks::system::JITCompiledCPP lib("#include \"current.h\"\nextern \"C\" int bar() { return 101; }");
  auto bar = lib.template Get<int (*)()>("bar");
  EXPECT_EQ(101, bar());
}

TEST(Syscalls, DLOpenHasSymlinkToCurrent) {
  if (!FLAGS_current_base_dir_for_dlopen_test.empty()) {
    current::bricks::system::JITCompiledCPP lib(
        "#include \"current.h\"\nextern \"C\" std::string test() { return \"passed\"; }",
        FLAGS_current_base_dir_for_dlopen_test);
    auto test = lib.template Get<std::string (*)()>("test");
    EXPECT_EQ("passed", test());
  } else {
    std::cout << "Test skipped as `--current_base_dir_for_dlopen_test` was not set." << std::endl;
  }
}

TEST(Syscalls, DLOpenTwoLibraries) {
  current::bricks::system::JITCompiledCPP lib1("extern \"C\" int Get42() { return 42; }");
  EXPECT_EQ(42, lib1.template Get<int(*)()>("Get42")());

  current::bricks::system::JITCompiledCPP lib2("extern \"C\" int Get101() { return 101; }");
  EXPECT_EQ(101, lib2.template Get<int(*)()>("Get101")());
}

TEST(Syscalls, DLOpenTwoLibrariesSameNamespaceCollisionNuance) {
  // clang-format off
  current::bricks::system::JITCompiledCPP lib1(R"(
    namespace magic {
      inline int N = 42;
    }
    extern "C" int Get42Correct() {
      return magic::N;
    }
  )");
  // clang-format on

  EXPECT_EQ(42, lib1.template Get<int(*)()>("Get42Correct")());

  // clang-format off
  current::bricks::system::JITCompiledCPP lib2(R"(
    namespace magic {
      inline int N = 101;
    }
    extern "C" int Get101WithANuance() {
      return magic::N;
    }
  )");
  // clang-format on

  EXPECT_NE(101, lib2.template Get<int(*)()>("Get101WithANuance")());
  EXPECT_EQ(42, lib2.template Get<int(*)()>("Get101WithANuance")());
}

TEST(Syscalls, DLOpenTwoLibrariesTwoNamespaces) {
  // clang-format off
  current::bricks::system::JITCompiledCPP lib1(R"(
    namespace magic42 {
      inline int N = 42;
    }
    extern "C" int Get42Correct() {
      return magic42::N;
    }
  )");
  // clang-format on

  EXPECT_EQ(42, lib1.template Get<int(*)()>("Get42Correct")());

  // clang-format off
  current::bricks::system::JITCompiledCPP lib2(R"(
    namespace magic101 {
      inline int N = 101;
    }
    extern "C" int Get101Correct() {
      return magic101::N;
    }
  )");
  // clang-format on

  EXPECT_EQ(101, lib2.template Get<int(*)()>("Get101Correct")());
}

TEST(Syscalls, DLOpenExceptions) {
  ASSERT_THROW(current::bricks::system::JITCompiledCPP("*"), current::bricks::system::CompilationException);
  {
    current::bricks::system::JITCompiledCPP lib("");  // An empty source is fine.
    ASSERT_THROW(lib.template Get<int (*)()>("bwahaha"), current::bricks::system::DLSymException);
  }
}
#endif  // CURRENT_WINDOWS
