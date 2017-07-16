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

#include "csv.h"

#include "../../bricks/file/file.h"
#include "../../bricks/dflags/dflags.h"
#include "../../TypeSystem/Serialization/json.h"
#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(csv_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

TEST(CSV, Smoke) {
  current::FileSystem::MkDir(FLAGS_csv_test_tmpdir, current::FileSystem::MkDirParameters::Silent);
  const std::string fn = current::FileSystem::JoinPath(FLAGS_csv_test_tmpdir, "test.csv");
  const auto persistence_file_remover = current::FileSystem::ScopedRmFile(fn);

  ASSERT_THROW(current::CSV<double>::ReadFile(fn), current::CSVFileNotFoundException);

  {
    current::FileSystem::WriteStringToFile("", fn.c_str());
    ASSERT_THROW(current::CSV<double>::ReadFile(fn), current::CSVFileFormatException);
    current::FileSystem::WriteStringToFile("\n", fn.c_str());
    ASSERT_THROW(current::CSV<double>::ReadFile(fn), current::CSVFileFormatException);
  }

  {
    current::FileSystem::WriteStringToFile("A,B,C\n1,2,3\n4,5,6\n7,8,9\n", fn.c_str());
    const auto csv = current::CSV<double>::ReadFile(fn);
    EXPECT_EQ("[\"A\",\"B\",\"C\"]", JSON(csv.header));
    EXPECT_EQ(3u, csv.data.size());
    EXPECT_EQ(3u, csv.data[0].size());
    EXPECT_EQ(3u, csv.data[1].size());
    EXPECT_EQ(3u, csv.data[2].size());
    EXPECT_EQ(1, csv.data[0][0]);
    EXPECT_EQ(2, csv.data[0][1]);
    EXPECT_EQ(3, csv.data[0][2]);
    EXPECT_EQ(4, csv.data[1][0]);
    EXPECT_EQ(5, csv.data[1][1]);
    EXPECT_EQ(6, csv.data[1][2]);
    EXPECT_EQ(7, csv.data[2][0]);
    EXPECT_EQ(8, csv.data[2][1]);
    EXPECT_EQ(9, csv.data[2][2]);
  }

  {
    current::FileSystem::WriteStringToFile("no,data,in,this,file\n", fn.c_str());
    const auto csv = current::CSV<double>::ReadFile(fn);
    EXPECT_EQ("[\"no\",\"data\",\"in\",\"this\",\"file\"]", JSON(csv.header));
    EXPECT_EQ(0u, csv.data.size());
  }

  {
    current::FileSystem::WriteStringToFile("wrong,number,of,columns\n1,2,3\n", fn.c_str());
    ASSERT_THROW(current::CSV<double>::ReadFile(fn), current::CSVFileFormatException);
  }

  {
    current::FileSystem::WriteStringToFile("X\r\n 1.0  \r\n  20e-1  \r\n  \t\t3\t\t \n", fn.c_str());
    const auto csv = current::CSV<double>::ReadFile(fn);
    EXPECT_EQ("[\"X\"]", JSON(csv.header));
    EXPECT_EQ(3u, csv.data.size());
    EXPECT_EQ(1u, csv.data[0].size());
    EXPECT_EQ(1u, csv.data[1].size());
    EXPECT_EQ(1u, csv.data[2].size());
    EXPECT_EQ(1, csv.data[0][0]);
    EXPECT_EQ(2, csv.data[1][0]);
    EXPECT_EQ(3, csv.data[2][0]);
  }

  {
    current::FileSystem::WriteStringToFile("just,a,header,with,no,newline,is,ok", fn.c_str());
    const auto csv = current::CSV<double>::ReadFile(fn);
    EXPECT_EQ("[\"just\",\"a\",\"header\",\"with\",\"no\",\"newline\",\"is\",\"ok\"]", JSON(csv.header));
  }
}
