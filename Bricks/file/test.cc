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

#include <algorithm>
#include <string>
#include <vector>

#include "file.h"

#include "../dflags/dflags.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

using bricks::FileSystem;
using bricks::FileException;
using bricks::DirDoesNotExistException;
using bricks::PathIsNotADirectoryException;
using bricks::DirIsNotEmptyException;

DEFINE_string(file_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

TEST(File, JoinPath) {
#ifndef BRICKS_WINDOWS
  EXPECT_EQ("foo/bar", FileSystem::JoinPath("foo", "bar"));
  EXPECT_EQ("foo/bar", FileSystem::JoinPath("foo/", "bar"));
  EXPECT_EQ("/foo/bar", FileSystem::JoinPath("/foo", "bar"));
  EXPECT_EQ("/foo/bar", FileSystem::JoinPath("/foo/", "bar"));
  EXPECT_EQ("/bar", FileSystem::JoinPath("foo", "/bar"));
  EXPECT_EQ("/bar", FileSystem::JoinPath("/foo", "/bar"));
  EXPECT_EQ("/bar", FileSystem::JoinPath("/", "bar"));
  EXPECT_EQ("/bar", FileSystem::JoinPath("/", "/bar"));
  EXPECT_EQ("bar", FileSystem::JoinPath("", "bar"));
  ASSERT_THROW(FileSystem::JoinPath("/foo/", ""), FileException);
#else
  EXPECT_EQ("foo\\bar", FileSystem::JoinPath("foo", "bar"));
  EXPECT_EQ("foo\\bar", FileSystem::JoinPath("foo\\", "bar"));
  EXPECT_EQ("\\foo\\bar", FileSystem::JoinPath("\\foo", "bar"));
  EXPECT_EQ("\\foo\\bar", FileSystem::JoinPath("\\foo\\", "bar"));
  EXPECT_EQ("\\bar", FileSystem::JoinPath("foo", "\\bar"));
  EXPECT_EQ("\\bar", FileSystem::JoinPath("\\foo", "\\bar"));
  EXPECT_EQ("\\bar", FileSystem::JoinPath("\\", "bar"));
  EXPECT_EQ("\\bar", FileSystem::JoinPath("\\", "\\bar"));
  EXPECT_EQ("bar", FileSystem::JoinPath("", "bar"));
  ASSERT_THROW(FileSystem::JoinPath("\\foo\\", ""), FileException);
#endif
}

TEST(File, GetFileExtension) {
#ifndef BRICKS_WINDOWS
  EXPECT_EQ("", FileSystem::GetFileExtension(""));
  EXPECT_EQ("", FileSystem::GetFileExtension("a"));
  EXPECT_EQ("b", FileSystem::GetFileExtension("a.b"));
  EXPECT_EQ("c", FileSystem::GetFileExtension("a.b.c"));
  EXPECT_EQ("a", FileSystem::GetFileExtension(".a"));
  EXPECT_EQ("a", FileSystem::GetFileExtension("..a"));
  EXPECT_EQ("", FileSystem::GetFileExtension("a/b"));
  EXPECT_EQ("b", FileSystem::GetFileExtension("a/.b"));
  EXPECT_EQ("", FileSystem::GetFileExtension("a/b/c"));
  EXPECT_EQ("c", FileSystem::GetFileExtension("a/b/.c"));
  EXPECT_EQ("a", FileSystem::GetFileExtension(".a"));
  EXPECT_EQ("", FileSystem::GetFileExtension("./a"));
  EXPECT_EQ("", FileSystem::GetFileExtension("../a"));
  EXPECT_EQ("a", FileSystem::GetFileExtension("../.a"));
  EXPECT_EQ("long_extension", FileSystem::GetFileExtension("long_name.long_extension"));
#else
  EXPECT_EQ("", FileSystem::GetFileExtension(""));
  EXPECT_EQ("", FileSystem::GetFileExtension("a"));
  EXPECT_EQ("b", FileSystem::GetFileExtension("a.b"));
  EXPECT_EQ("c", FileSystem::GetFileExtension("a.b.c"));
  EXPECT_EQ("a", FileSystem::GetFileExtension(".a"));
  EXPECT_EQ("a", FileSystem::GetFileExtension("..a"));
  EXPECT_EQ("", FileSystem::GetFileExtension("a\\b"));
  EXPECT_EQ("b", FileSystem::GetFileExtension("a\\.b"));
  EXPECT_EQ("", FileSystem::GetFileExtension("a\\b\\c"));
  EXPECT_EQ("c", FileSystem::GetFileExtension("a\\b\\.c"));
  EXPECT_EQ("a", FileSystem::GetFileExtension(".a"));
  EXPECT_EQ("", FileSystem::GetFileExtension(".\\a"));
  EXPECT_EQ("", FileSystem::GetFileExtension("..\\a"));
  EXPECT_EQ("a", FileSystem::GetFileExtension("..\\.a"));
  EXPECT_EQ("long_extension", FileSystem::GetFileExtension("long_name.long_extension"));
#endif
}

TEST(File, FileStringOperations) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string fn = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "tmp");

  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  ASSERT_THROW(FileSystem::RmFile(fn), FileException);
  ASSERT_THROW(FileSystem::ReadFileAsString(fn), FileException);
  ASSERT_THROW(FileSystem::IsDir(fn), FileException);

  FileSystem::WriteStringToFile("PASSED", fn.c_str());
  EXPECT_EQ("PASSED", FileSystem::ReadFileAsString(fn));
  EXPECT_FALSE(FileSystem::IsDir(fn));

  FileSystem::WriteStringToFile("ANOTHER", fn.c_str());
  FileSystem::WriteStringToFile(" TEST", fn.c_str(), true);
  EXPECT_EQ("ANOTHER TEST", FileSystem::ReadFileAsString(fn));

  FileSystem::RmFile(fn);
  FileSystem::MkDir(fn);
  EXPECT_TRUE(FileSystem::IsDir(fn));
  ASSERT_THROW(FileSystem::WriteStringToFile("not so fast", fn.c_str()), FileException);
  FileSystem::RmDir(fn);
}

TEST(File, GetFileSize) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string fn = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "size");

  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  ASSERT_THROW(FileSystem::GetFileSize(fn), FileException);

  FileSystem::WriteStringToFile("four", fn.c_str());
  EXPECT_EQ(4ull, FileSystem::GetFileSize(fn));
}

TEST(File, RenameFile) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string fn1 = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "one");
  const std::string fn2 = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "two");

  FileSystem::RmFile(fn1, FileSystem::RmFileParameters::Silent);
  FileSystem::RmFile(fn2, FileSystem::RmFileParameters::Silent);
  ASSERT_THROW(FileSystem::RenameFile(fn1, fn2), FileException);

  FileSystem::WriteStringToFile("data", fn1.c_str());

  EXPECT_EQ("data", FileSystem::ReadFileAsString(fn1));
  ASSERT_THROW(FileSystem::ReadFileAsString(fn2), FileException);

  FileSystem::RenameFile(fn1, fn2);

  ASSERT_THROW(FileSystem::ReadFileAsString(fn1), FileException);
  EXPECT_EQ("data", FileSystem::ReadFileAsString(fn2));

  ASSERT_THROW(FileSystem::RenameFile(fn1, fn2), FileException);
}

TEST(File, DirOperations) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string& dir = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "dir");
  const std::string fn = FileSystem::JoinPath(dir, "file");

  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);

  ASSERT_THROW(FileSystem::WriteStringToFile("test", fn.c_str()), FileException);
  ASSERT_THROW(FileSystem::RmDir(dir), DirDoesNotExistException);

  FileSystem::MkDir(dir);
  ASSERT_THROW(FileSystem::MkDir(dir), FileException);
  FileSystem::MkDir(dir, FileSystem::MkDirParameters::Silent);

  FileSystem::WriteStringToFile("test", fn.c_str());
  EXPECT_EQ("test", FileSystem::ReadFileAsString(fn));

  ASSERT_THROW(FileSystem::GetFileSize(dir), FileException);

  ASSERT_THROW(FileSystem::RmDir(dir), DirIsNotEmptyException);
  FileSystem::RmFile(fn);
  FileSystem::RmDir(dir);

  ASSERT_THROW(FileSystem::WriteStringToFile("dir does not exist", fn.c_str()), FileException);
}

TEST(File, ScopedRmFile) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string fn = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "tmp");

  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  ASSERT_THROW(FileSystem::ReadFileAsString(fn), FileException);
  {
    const auto file_remover = FileSystem::ScopedRmFile(fn);
    FileSystem::WriteStringToFile("PASSED", fn.c_str());
    EXPECT_EQ("PASSED", FileSystem::ReadFileAsString(fn));
  }
  ASSERT_THROW(FileSystem::ReadFileAsString(fn), FileException);
}

TEST(File, ScanDir) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string& dir = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "dir_to_scan");
  const std::string fn1 = FileSystem::JoinPath(dir, "one");
  const std::string fn2 = FileSystem::JoinPath(dir, "two");

  FileSystem::RmFile(fn1, FileSystem::RmFileParameters::Silent);
  FileSystem::RmFile(fn2, FileSystem::RmFileParameters::Silent);
  FileSystem::RmDir(FileSystem::JoinPath(dir, "subdir"), FileSystem::RmDirParameters::Silent);
  FileSystem::RmFile(dir, FileSystem::RmFileParameters::Silent);
  FileSystem::RmDir(dir, FileSystem::RmDirParameters::Silent);

  struct Scanner {
    explicit Scanner(const std::string& dir, bool return_code = true) : dir_(dir), return_code_(return_code) {}
    Scanner(const Scanner&) = delete;  // Make sure ScanDir()/ScanDirUntil() don't copy the argument.
    bool operator()(const std::string& file_name) {
      files_.push_back(
          std::make_pair(file_name, FileSystem::ReadFileAsString(FileSystem::JoinPath(dir_, file_name))));
      return return_code_;
    }
    const std::string dir_;
    const bool return_code_;
    std::vector<std::pair<std::string, std::string>> files_;
  };

  Scanner scanner_before(dir);
  ASSERT_THROW(FileSystem::ScanDir(dir, scanner_before), DirDoesNotExistException);
  ASSERT_THROW(FileSystem::ScanDirUntil(dir, scanner_before), DirDoesNotExistException);

#ifndef BRICKS_WINDOWS
  {
    // TODO(dkorolev): Perhaps support this exception type on Windows as well.
    const auto file_remover = FileSystem::ScopedRmFile(dir);
    FileSystem::WriteStringToFile("This is a file, not a directory!", dir.c_str());
    ASSERT_THROW(FileSystem::ScanDir(dir, scanner_before), PathIsNotADirectoryException);
    ASSERT_THROW(FileSystem::ScanDirUntil(dir, scanner_before), PathIsNotADirectoryException);
  }
#endif

  FileSystem::MkDir(dir);
  FileSystem::WriteStringToFile("foo", fn1.c_str());
  FileSystem::WriteStringToFile("bar", fn2.c_str());
  FileSystem::MkDir(FileSystem::JoinPath(dir, "subdir"));

  Scanner scanner_after(dir);
  FileSystem::ScanDir(dir, scanner_after);
  ASSERT_EQ(2u, scanner_after.files_.size());
  std::sort(scanner_after.files_.begin(), scanner_after.files_.end());
  EXPECT_EQ("one", scanner_after.files_[0].first);
  EXPECT_EQ("foo", scanner_after.files_[0].second);
  EXPECT_EQ("two", scanner_after.files_[1].first);
  EXPECT_EQ("bar", scanner_after.files_[1].second);

  Scanner scanner_after_until(dir, false);
  FileSystem::ScanDirUntil(dir, scanner_after_until);
  ASSERT_EQ(1u, scanner_after_until.files_.size());
  EXPECT_TRUE(scanner_after_until.files_[0].first == "one" || scanner_after_until.files_[0].first == "two");
  EXPECT_EQ((scanner_after_until.files_[0].first == "one" ? "foo" : "bar"),
            scanner_after_until.files_[0].second);

  FileSystem::RmDir(FileSystem::JoinPath(dir, "subdir"), FileSystem::RmDirParameters::Silent);
}

TEST(File, RmDirRecursive) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string& dir_x = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "x");
  const std::string& f1 = FileSystem::JoinPath(dir_x, "1");
  const std::string& dir_y = FileSystem::JoinPath(dir_x, "y");
  const std::string& f2 = FileSystem::JoinPath(dir_y, "2");
  const std::string& dir_z = FileSystem::JoinPath(dir_y, "z");
  const std::string& f3 = FileSystem::JoinPath(dir_z, "3");
  const std::string& f4 = FileSystem::JoinPath(dir_z, "4");

  FileSystem::MkDir(dir_x);
  FileSystem::WriteStringToFile("1", f1.c_str());
  FileSystem::MkDir(dir_y);
  FileSystem::WriteStringToFile("2", f2.c_str());
  FileSystem::MkDir(dir_z);
  FileSystem::WriteStringToFile("3", f3.c_str());
  FileSystem::WriteStringToFile("4", f4.c_str());

  FileSystem::RmDir(dir_x, FileSystem::RmDirParameters::ThrowExceptionOnError, FileSystem::RmDirRecursive::Yes);

  ASSERT_THROW(FileSystem::WriteStringToFile("test", f3.c_str()), FileException);
  ASSERT_THROW(FileSystem::WriteStringToFile("test", f2.c_str()), FileException);
  ASSERT_THROW(FileSystem::WriteStringToFile("test", f1.c_str()), FileException);
  ASSERT_THROW(FileSystem::RmDir(
                   dir_x, FileSystem::RmDirParameters::ThrowExceptionOnError, FileSystem::RmDirRecursive::Yes),
               DirDoesNotExistException);
}

TEST(File, ScopedRmDir) {
  // Required for Windows tests.
  FileSystem::MkDir(FLAGS_file_test_tmpdir, FileSystem::MkDirParameters::Silent);

  const std::string& dir_x = FileSystem::JoinPath(FLAGS_file_test_tmpdir, "x");
  const std::string& dir_y = FileSystem::JoinPath(dir_x, "y");
  const std::string& dir_z = FileSystem::JoinPath(dir_y, "z");
  const std::string& fn = FileSystem::JoinPath(dir_z, "test");

  FileSystem::RmFile(fn, FileSystem::RmFileParameters::Silent);
  ASSERT_THROW(FileSystem::ReadFileAsString(fn), FileException);
  {
    const auto dir_remover = FileSystem::ScopedRmDir(dir_x);
    FileSystem::MkDir(dir_x);
    FileSystem::MkDir(dir_y);
    FileSystem::MkDir(dir_z);
    FileSystem::WriteStringToFile("PASSED", fn.c_str());
    EXPECT_EQ("PASSED", FileSystem::ReadFileAsString(fn));
  }
  ASSERT_THROW(FileSystem::ReadFileAsString(fn), FileException);
  ASSERT_THROW(FileSystem::RmDir(dir_z), DirDoesNotExistException);
  ASSERT_THROW(FileSystem::RmDir(dir_y), DirDoesNotExistException);
  ASSERT_THROW(FileSystem::RmDir(dir_x), DirDoesNotExistException);
}
