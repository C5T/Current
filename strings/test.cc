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

#include <vector>
#include <string>
#include <set>

#include "util.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

using bricks::strings::Printf;
using bricks::strings::FixedSizeSerializer;
using bricks::strings::PackToString;
using bricks::strings::UnpackFromString;
using bricks::strings::CompileTimeStringLength;
using bricks::strings::Join;
using bricks::strings::Split;
using bricks::strings::SplitIntoKeyValuePairs;
using bricks::strings::TrimMode;
using bricks::strings::KeyValueThrowMode;
using bricks::strings::KeyValueNoValueException;
using bricks::strings::KeyValueMultipleValuesException;

TEST(StringPrintf, SmokeTest) {
  EXPECT_EQ("Test: 42, 'Hello', 0000ABBA", Printf("Test: %d, '%s', %08X", 42, "Hello", 0xabba));
}

TEST(FixedSizeSerializer, UInt16) {
  EXPECT_EQ(5, FixedSizeSerializer<uint16_t>::size_in_bytes);
  // Does not fit signed 16-bit, requires unsigned.
  EXPECT_EQ("54321", FixedSizeSerializer<uint16_t>::PackToString(54321));
  EXPECT_EQ(54321, FixedSizeSerializer<uint16_t>::UnpackFromString("54321"));
}

TEST(FixedSizeSerializer, UInt32) {
  EXPECT_EQ(10, FixedSizeSerializer<uint32_t>::size_in_bytes);
  // Does not fit signed 32-bit, requires unsigned.
  EXPECT_EQ("3987654321", FixedSizeSerializer<uint32_t>::PackToString(3987654321));
  EXPECT_EQ(3987654321, FixedSizeSerializer<uint32_t>::UnpackFromString("3987654321"));
}

TEST(FixedSizeSerializer, UInt64) {
  EXPECT_EQ(20, FixedSizeSerializer<uint64_t>::size_in_bytes);
  uint64_t magic = 1e19;
  magic += 42;
  // Does not fit signed 64-bit.
  EXPECT_EQ("10000000000000000042", FixedSizeSerializer<uint64_t>::PackToString(magic));
  EXPECT_EQ(magic, FixedSizeSerializer<uint64_t>::UnpackFromString("10000000000000000042"));
}

TEST(FixedSizeSerializer, ImplicitSyntax) {
  {
    uint32_t x;
    EXPECT_EQ(42u, UnpackFromString("42", x));
  }
  {
    uint16_t x;
    EXPECT_EQ(10000u, UnpackFromString("10000", x));
  }
  {
    uint16_t x = 42;
    EXPECT_EQ("00042", PackToString(x));
  }
  {
    uint64_t x = static_cast<int64_t>(1e18);
    EXPECT_EQ("01000000000000000000", PackToString(x));
  }
}

static const char global_string[] = "magic";

TEST(Util, CompileTimeStringLength) {
  const char local_string[] = "foo";
  static const char local_static_string[] = "blah";
  EXPECT_EQ(3u, CompileTimeStringLength(local_string));
  EXPECT_EQ(4u, CompileTimeStringLength(local_static_string));
  EXPECT_EQ(5u, CompileTimeStringLength(global_string));
}

TEST(JoinAndSplit, Join) {
  EXPECT_EQ("one,two,three", Join({"one", "two", "three"}, ','));
  EXPECT_EQ("onetwothree", Join({"one", "two", "three"}, ""));
  EXPECT_EQ("one, two, three", Join({"one", "two", "three"}, ", "));
  EXPECT_EQ("one, two, three", Join({"one", "two", "three"}, std::string(", ")));
  EXPECT_EQ("", Join({}, ' '));
  EXPECT_EQ("", Join({}, " "));

  EXPECT_EQ("a,b,c,b", Join(std::vector<std::string>({"a", "b", "c", "b"}), ','));
  EXPECT_EQ("a,b,c", Join(std::set<std::string>({"a", "b", "c", "b"}), ','));
  EXPECT_EQ("a,b,b,c", Join(std::multiset<std::string>({"a", "b", "c", "b"}), ','));
}

TEST(JoinAndSplit, Split) {
  EXPECT_EQ("one two three", Join(Split("one,two,three", ','), ' '));
  EXPECT_EQ("one two three four", Join(Split("one,two|three,four", ",|"), ' '));
  EXPECT_EQ("one two three four", Join(Split("one,two|three,four", std::string(",|")), ' '));
  EXPECT_EQ("one,two three,four", Join(Split("one,two|three,four", '|'), ' '));
  EXPECT_EQ("one,two three,four", Join(Split("one,two|three,four", "|"), ' '));
  EXPECT_EQ("one,two three,four", Join(Split("one,two|three,four", std::string("|")), ' '));

  EXPECT_EQ("one two three", Join(Split(",,one,,,two,,,three,,", ','), ' '));
  EXPECT_EQ("  one   two   three  ", Join(Split(",,one,,,two,,,three,,", ',', TrimMode::NoTrim), ' '));
}

TEST(JoinAndSplit, FunctionalSplit) {
  {
    std::string result;
    Split("one,two,three", ',', [&result](const std::string& s) { result += s + '\n'; });
    EXPECT_EQ("one\ntwo\nthree\n", result);
  }
  {
    std::string result;
    Split("one,two,three", ',', [&result](std::string&& s) { result += s + '\n'; });
    EXPECT_EQ("one\ntwo\nthree\n", result);
  }
  {
    std::string result;
    struct Helper {
      std::string& result_;
      explicit Helper(std::string& result) : result_(result) {}
      void operator()(const std::string& s) const { result_ += s + '\n'; }
      Helper(Helper&) = delete;
      Helper(Helper&&) = delete;
      void operator=(const Helper&) = delete;
      void operator=(Helper&&) = delete;
    };
    Helper helper(result);
    Split("one,two,three", ',', [&result](const std::string& s) { result += s + '\n'; });
    EXPECT_EQ("one\ntwo\nthree\n", result);
  }
  {
    std::string result;
    struct Helper {
      std::string& result_;
      explicit Helper(std::string& result) : result_(result) {}
      void operator()(std::string&& s) const { result_ += s + '\n'; }
      Helper(Helper&) = delete;
      Helper(Helper&&) = delete;
      void operator=(const Helper&) = delete;
      void operator=(Helper&&) = delete;
    };
    Helper helper(result);
    Split("one,two,three", ',', [&result](const std::string& s) { result += s + '\n'; });
    EXPECT_EQ("one\ntwo\nthree\n", result);
  }
}

TEST(JoinAndSplit, SplitIntoKeyValuePairs) {
  const auto result = SplitIntoKeyValuePairs("one=1,two=2", ',', '=');
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("one", result[0].first);
  EXPECT_EQ("1", result[0].second);
  EXPECT_EQ("two", result[1].first);
  EXPECT_EQ("2", result[1].second);
}

TEST(JoinAndSplit, SplitIntoKeyValuePairsExceptions) {
  const auto default_is_to_not_throw = SplitIntoKeyValuePairs("test,foo=bar=baz,one=1,two=2,passed", ',', '=');
  ASSERT_EQ(2u, default_is_to_not_throw.size());
  EXPECT_EQ("one", default_is_to_not_throw[0].first);
  EXPECT_EQ("1", default_is_to_not_throw[0].second);
  EXPECT_EQ("two", default_is_to_not_throw[1].first);
  EXPECT_EQ("2", default_is_to_not_throw[1].second);
  const auto correct_case = SplitIntoKeyValuePairs("one=1,two=2", ',', '=', KeyValueThrowMode::Throw);
  ASSERT_EQ(2u, correct_case.size());
  EXPECT_EQ("one", correct_case[0].first);
  EXPECT_EQ("1", correct_case[0].second);
  EXPECT_EQ("two", correct_case[1].first);
  EXPECT_EQ("2", correct_case[1].second);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo", ',', '=', KeyValueThrowMode::Throw), KeyValueNoValueException);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo=bar=baz", ',', '=', KeyValueThrowMode::Throw),
               KeyValueMultipleValuesException);
}
