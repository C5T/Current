/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#define CURRENT_MOCK_TIME

#include "config.h"

#include "../../bricks/dflags/dflags.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(self_modifying_config_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

CURRENT_STRUCT(UnitTestSelfModifyingConfig) {
  CURRENT_FIELD(x, int32_t, 0);
  CURRENT_CONSTRUCTOR(UnitTestSelfModifyingConfig)(int32_t x = 0) : x(x) {}
};

TEST(SelfModifyingConfig, Smoke) {
  current::time::ResetToZero();

  const std::string config_filename =
      current::FileSystem::JoinPath(FLAGS_self_modifying_config_test_tmpdir, CurrentTestName() + "Config");
  const auto original_file_remover = current::FileSystem::ScopedRmFile(config_filename);

  const std::string historical_filename = config_filename + ".19830816-000000";
  const auto historical_file_remover = current::FileSystem::ScopedRmFile(historical_filename);

  const std::string another_historical_filename = config_filename + ".20150816-000000";
  const auto another_historical_file_remover = current::FileSystem::ScopedRmFile(another_historical_filename);

  current::FileSystem::WriteStringToFile(JSON(UnitTestSelfModifyingConfig(1)), config_filename.c_str());

  current::time::SetNow(
      current::IMFFixDateTimeStringToTimestamp("Tue, 16 Aug 1983 00:00:00 GMT"));  // I was born. -- D.K.

  current::SelfModifyingConfig<UnitTestSelfModifyingConfig> config(config_filename);
  EXPECT_EQ(1, config.Config().x);

  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(1)), current::FileSystem::ReadFileAsString(config_filename));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(1)), current::FileSystem::ReadFileAsString(historical_filename));

  current::time::SetNow(
      current::IMFFixDateTimeStringToTimestamp("Sun, 16 Aug 2015 00:00:00 GMT"));  // I turned 32. -- D.K.
  config.Update(UnitTestSelfModifyingConfig(2));

  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(1)), current::FileSystem::ReadFileAsString(historical_filename));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(1)), current::FileSystem::ReadFileAsString(another_historical_filename));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(2)), current::FileSystem::ReadFileAsString(config_filename));

  config.Update(UnitTestSelfModifyingConfig(3));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(1)), current::FileSystem::ReadFileAsString(historical_filename));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(2)), current::FileSystem::ReadFileAsString(another_historical_filename));
  EXPECT_EQ(JSON(UnitTestSelfModifyingConfig(3)), current::FileSystem::ReadFileAsString(config_filename));
}

TEST(SelfModifyingConfig, ReadFileException) {
  current::time::ResetToZero();

  const std::string config_filename =
      current::FileSystem::JoinPath(FLAGS_self_modifying_config_test_tmpdir, CurrentTestName() + "Config");
  const auto original_file_remover = current::FileSystem::ScopedRmFile(config_filename);

  const std::string historical_filename = config_filename + ".19830816-000000";
  const auto historical_file_remover = current::FileSystem::ScopedRmFile(historical_filename);

  current::time::SetNow(
      current::IMFFixDateTimeStringToTimestamp("Tue, 16 Aug 1983 00:00:00 GMT"));  // I was born. -- D.K.

  ASSERT_THROW(static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename)),
               current::SelfModifyingConfigReadFileException);
  try {
    static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename));
  } catch (const current::SelfModifyingConfigReadFileException& e) {
    EXPECT_EQ("SelfModifyingConfigReadFileException(filename_)", e.Caller());
    EXPECT_EQ(".current/ReadFileExceptionConfig", e.OriginalDescription());
    ExpectStringEndsWith("config.h", e.File());
    EXPECT_EQ(80, e.Line());
  }
}

TEST(SelfModifyingConfig, ParseJSONException) {
  current::time::ResetToZero();

  const std::string config_filename =
      current::FileSystem::JoinPath(FLAGS_self_modifying_config_test_tmpdir, CurrentTestName() + "Config");
  const auto original_file_remover = current::FileSystem::ScopedRmFile(config_filename);

  const std::string historical_filename = config_filename + ".19830816-000000";
  const auto historical_file_remover = current::FileSystem::ScopedRmFile(historical_filename);

  current::time::SetNow(
      current::IMFFixDateTimeStringToTimestamp("Tue, 16 Aug 1983 00:00:00 GMT"));  // I was born. -- D.K.

  current::FileSystem::WriteStringToFile("De Louboutin.", config_filename.c_str());

  ASSERT_THROW(static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename)),
               current::SelfModifyingConfigParseJSONException);
  try {
    static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename));
  } catch (const current::SelfModifyingConfigParseJSONException& e) {
    EXPECT_EQ("SelfModifyingConfigParseJSONException(what)", e.Caller());
    EXPECT_EQ("File doesn't contain a valid JSON: 'De Louboutin.'", e.OriginalDescription());
    ExpectStringEndsWith("config.h", e.File());
    EXPECT_EQ(88, e.Line());
  }
}

TEST(SelfModifyingConfig, WriteFileException) {
  current::time::ResetToZero();

  const std::string config_filename =
      current::FileSystem::JoinPath(FLAGS_self_modifying_config_test_tmpdir, CurrentTestName() + "Config");
  const auto original_file_remover = current::FileSystem::ScopedRmFile(config_filename);

  // Block file creation by creating a directory instead.
  const std::string historical_filename = config_filename + ".19830816-000000";
  const auto historical_file_remover = current::FileSystem::ScopedRmDir(historical_filename);
  current::FileSystem::MkDir(historical_filename, current::FileSystem::MkDirParameters::Silent);

  current::time::SetNow(
      current::IMFFixDateTimeStringToTimestamp("Tue, 16 Aug 1983 00:00:00 GMT"));  // I was born. -- D.K.

  current::FileSystem::WriteStringToFile(JSON(UnitTestSelfModifyingConfig(4)), config_filename.c_str());

  ASSERT_THROW(static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename)),
               current::SelfModifyingConfigWriteFileException);
  try {
    static_cast<void>(std::make_unique<current::SelfModifyingConfig<UnitTestSelfModifyingConfig>>(config_filename));
  } catch (const current::SelfModifyingConfigWriteFileException& e) {
    EXPECT_EQ("SelfModifyingConfigWriteFileException(new_filename)", e.Caller());
    EXPECT_EQ(".current/WriteFileExceptionConfig.19830816-000000", e.OriginalDescription());
    ExpectStringEndsWith("config.h", e.File());
    EXPECT_EQ(103, e.Line());
  }
}
