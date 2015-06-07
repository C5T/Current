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

#include "../3party/gtest/gtest-main.h"

using bricks::strings::Printf;
using bricks::strings::FixedSizeSerializer;
using bricks::strings::PackToString;
using bricks::strings::UnpackFromString;
using bricks::strings::CompileTimeStringLength;
using bricks::strings::Trim;
using bricks::strings::FromString;
using bricks::strings::ToLower;
using bricks::strings::ToUpper;
using bricks::strings::Join;
using bricks::strings::Split;
using bricks::strings::SplitIntoKeyValuePairs;
using bricks::strings::EmptyFields;
using bricks::strings::KeyValueParsing;
using bricks::strings::KeyValueNoValueException;
using bricks::strings::KeyValueMultipleValuesException;
using bricks::strings::ByWhitespace;
using bricks::strings::ByLines;
using bricks::strings::SlowEditDistance;
using bricks::strings::FastEditDistance;
using bricks::strings::Chunk;
using bricks::strings::UniqueChunk;
using bricks::strings::ChunkDB;

TEST(StringPrintf, SmokeTest) {
  EXPECT_EQ("Test: 42, 'Hello', 0000ABBA", Printf("Test: %d, '%s', %08X", 42, "Hello", 0xabba));
  EXPECT_EQ(1024u * 5, Printf("%s", std::string(10000, 'A').c_str()).length());
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
  uint64_t magic = static_cast<uint64_t>(1e19);
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

TEST(Util, Trim) {
  EXPECT_EQ("one", Trim(" one "));
  EXPECT_EQ("one", Trim(std::string(" one ")));
  EXPECT_EQ("two", Trim("   \t\n\t\n\t\r\n   two   \t\n\t\n\t\r\n   "));
  EXPECT_EQ("two", Trim(std::string("   \t\n\t\n\t\r\n   two   \t\n\t\n\t\r\n   ")));
  EXPECT_EQ("3 \t\r\n 4", Trim("   \t\n\t\n\t\r\n   3 \t\r\n 4   \t\n\t\n\t\r\n   "));
  EXPECT_EQ("3 \t\r\n 4", Trim(std::string("   \t\n\t\n\t\r\n   3 \t\r\n 4   \t\n\t\n\t\r\n   ")));
  EXPECT_EQ("", Trim(""));
  EXPECT_EQ("", Trim(std::string("")));
  EXPECT_EQ("", Trim(" \t\r\n\t "));
  EXPECT_EQ("", Trim(std::string(" \t\r\n\t ")));
}

TEST(Util, FromString) {
  EXPECT_EQ(1, FromString<int>("1"));

  EXPECT_EQ(32767, static_cast<int>(FromString<int16_t>("32767")));
  EXPECT_EQ(65535, static_cast<int>(FromString<uint16_t>("65535")));

  double tmp;
  EXPECT_EQ(0.5, FromString("0.5", tmp));
  EXPECT_EQ(0.5, tmp);

  EXPECT_EQ(0u, FromString<size_t>(""));
  EXPECT_EQ(0u, FromString<size_t>("foo"));
  EXPECT_EQ(0u, FromString<size_t>("\n"));

  EXPECT_EQ(0.0, FromString<double>(""));
  EXPECT_EQ(0.0, FromString<double>("bar"));
  EXPECT_EQ(0.0, FromString<double>("\t"));
}

TEST(Util, ToUpperAndToLower) {
  EXPECT_EQ("test passed", ToLower("TeSt pAsSeD"));
  EXPECT_EQ("TEST PASSED", ToUpper("TeSt pAsSeD"));
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
  EXPECT_EQ("  one   two   three  ", Join(Split(",,one,,,two,,,three,,", ',', EmptyFields::Keep), ' '));

  EXPECT_EQ("one two three", Join(Split<ByWhitespace>("one two three"), ' '));
  EXPECT_EQ("one two three", Join(Split<ByWhitespace>("\t \tone\t \ttwo\t \tthree\t \t"), ' '));

  EXPECT_EQ("one two|three", Join(Split<ByLines>("one two\nthree"), '|'));
  EXPECT_EQ("one|two three", Join(Split<ByLines>("\r\n\n\r\none\n\r\n\n\r\ntwo three"), '|'));

  // Note that `Split` on a predicate splits on the characters for which the predicate returns `false`,
  // and keeps the characters where the predicate returns `true`.
  // This way, `Split` on `::isalpha` or `::isalnum` makes perfect sense.
  EXPECT_EQ("1 2 3 4 5", Join(Split("1 a2b\n3\n\n4\n\n&5$", [](char c) { return ::isdigit(c); }), ' '));
  EXPECT_EQ("ab c d e123", Join(Split("ab'c d--e123", ::isalnum), ' '));
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
    Split("one,two,three", ',', helper);
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
    Split("one,two,three", ',', helper);
    EXPECT_EQ("one\ntwo\nthree\n", result);
  }
}

TEST(JoinAndSplit, SplitIntoKeyValuePairs) {
  const auto result = SplitIntoKeyValuePairs("one=1,two=2", '=', ',');
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("one", result[0].first);
  EXPECT_EQ("1", result[0].second);
  EXPECT_EQ("two", result[1].first);
  EXPECT_EQ("2", result[1].second);
}

TEST(JoinAndSplit, SplitIntoKeyValuePairsWithWhitespaceBetweenPairs) {
  const auto result = SplitIntoKeyValuePairs("\t\n \tone=1\t\n \ttwo=2\t\n \t", '=');
  ASSERT_EQ(2u, result.size());
  EXPECT_EQ("one", result[0].first);
  EXPECT_EQ("1", result[0].second);
  EXPECT_EQ("two", result[1].first);
  EXPECT_EQ("2", result[1].second);
}

TEST(JoinAndSplit, SplitIntoKeyValuePairsExceptions) {
  const auto default_is_to_not_throw = SplitIntoKeyValuePairs("test,foo=bar=baz,one=1,two=2,passed", '=', ',');
  ASSERT_EQ(2u, default_is_to_not_throw.size());
  EXPECT_EQ("one", default_is_to_not_throw[0].first);
  EXPECT_EQ("1", default_is_to_not_throw[0].second);
  EXPECT_EQ("two", default_is_to_not_throw[1].first);
  EXPECT_EQ("2", default_is_to_not_throw[1].second);
  const auto correct_case = SplitIntoKeyValuePairs("one=1,two=2", '=', ',', KeyValueParsing::Throw);
  ASSERT_EQ(2u, correct_case.size());
  EXPECT_EQ("one", correct_case[0].first);
  EXPECT_EQ("1", correct_case[0].second);
  EXPECT_EQ("two", correct_case[1].first);
  EXPECT_EQ("2", correct_case[1].second);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo", '=', ',', KeyValueParsing::Throw), KeyValueNoValueException);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo=bar=baz", '=', ',', KeyValueParsing::Throw),
               KeyValueMultipleValuesException);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo", '=', KeyValueParsing::Throw), KeyValueNoValueException);
  ASSERT_THROW(SplitIntoKeyValuePairs("foo=bar=baz", '=', KeyValueParsing::Throw),
               KeyValueMultipleValuesException);
}

TEST(EditDistance, Smoke) {
  EXPECT_EQ(0u, SlowEditDistance("foo", "foo"));
  EXPECT_EQ(3u, SlowEditDistance("foo", ""));
  EXPECT_EQ(3u, SlowEditDistance("", "foo"));
  EXPECT_EQ(3u, SlowEditDistance("foo", "bar"));
  EXPECT_EQ(1u, SlowEditDistance("foo", "zoo"));
  EXPECT_EQ(1u, SlowEditDistance("foo", "fwo"));
  EXPECT_EQ(1u, SlowEditDistance("foo", "foe"));
  EXPECT_EQ(1u, SlowEditDistance("zoo", "foo"));
  EXPECT_EQ(1u, SlowEditDistance("fwo", "foo"));
  EXPECT_EQ(1u, SlowEditDistance("foe", "foo"));
  EXPECT_EQ(1u, SlowEditDistance("foo", "fo"));
  EXPECT_EQ(1u, SlowEditDistance("foo", "oo"));

  EXPECT_EQ(0u, FastEditDistance("foo", "foo", 10u));
  EXPECT_EQ(3u, FastEditDistance("foo", "", 10u));
  EXPECT_EQ(3u, FastEditDistance("", "foo", 10u));
  EXPECT_EQ(3u, FastEditDistance("foo", "bar", 10u));
  EXPECT_EQ(1u, FastEditDistance("foo", "zoo", 10u));
  EXPECT_EQ(1u, FastEditDistance("foo", "fwo", 10u));
  EXPECT_EQ(1u, FastEditDistance("foo", "foe", 10u));
  EXPECT_EQ(1u, FastEditDistance("zoo", "foo", 10u));
  EXPECT_EQ(1u, FastEditDistance("fwo", "foo", 10u));
  EXPECT_EQ(1u, FastEditDistance("foe", "foo", 10u));
  EXPECT_EQ(1u, FastEditDistance("foo", "fo", 10u));
  EXPECT_EQ(1u, FastEditDistance("foo", "oo", 10u));
}

TEST(EditDistance, MaxOffset1) {
  // Max. offset of 1 is fine, max. offset 0 is per-char comparison.
  EXPECT_EQ(2u, SlowEditDistance("abcde", "bcdef"));
  EXPECT_EQ(2u, FastEditDistance("abcde", "bcdef", 1u));
  EXPECT_EQ(5u, FastEditDistance("abcde", "bcdef", 0u));
}

TEST(EditDistance, MaxOffset2) {
  // Max. offset of 2 is fine, max. offset of 1 is same as max. offset of 0, which is per-char comparison.
  EXPECT_EQ(4u, SlowEditDistance("01234567", "23456789"));
  EXPECT_EQ(4u, FastEditDistance("01234567", "23456789", 2u));
  EXPECT_EQ(8u, FastEditDistance("01234567", "23456789", 1u));
  EXPECT_EQ(8u, FastEditDistance("01234567", "23456789", 0u));
}

TEST(EditDistance, StringsOfTooDifferentLength) {
  // When the strings are of too different lengths, `FastEditDistance` can't do anything.
  EXPECT_EQ(6u, SlowEditDistance("foo", "foobarbaz"));
  EXPECT_EQ(6u, SlowEditDistance("foobarbaz", "baz"));
  EXPECT_EQ(6u, FastEditDistance("foo", "foobarbaz", 6u));
  EXPECT_EQ(6u, FastEditDistance("foobarbaz", "baz", 6u));
  EXPECT_EQ(static_cast<size_t>(-1), FastEditDistance("foo", "foobarbaz", 5u));
  EXPECT_EQ(static_cast<size_t>(-1), FastEditDistance("foobarbaz", "baz", 5u));
}

TEST(Chunk, Smoke) {
  Chunk foo("foo", 3);
  EXPECT_FALSE(foo.empty());
  EXPECT_EQ(3u, foo.length());
  EXPECT_EQ(0, ::strcmp("foo", foo.c_str()));

  Chunk bar("bar\0baz", 3);
  EXPECT_FALSE(bar.empty());
  EXPECT_EQ(3u, bar.length());
  EXPECT_EQ(0, ::strcmp("bar", bar.c_str()));

  Chunk empty;
  EXPECT_TRUE(empty.empty());
  EXPECT_EQ(0u, empty.length());

  Chunk foo_copy = foo;
  Chunk bar_copy = "meh";
  bar_copy = bar;

  EXPECT_TRUE(foo_copy.HasPrefix(foo));
  EXPECT_TRUE(foo_copy.HasPrefix("foo"));
  EXPECT_TRUE(foo_copy.HasPrefix("fo"));
  EXPECT_TRUE(foo_copy.HasPrefix("f"));
  EXPECT_TRUE(foo_copy.HasPrefix(""));
  EXPECT_FALSE(foo_copy.HasPrefix(bar));
  EXPECT_FALSE(foo_copy.HasPrefix("bar"));
  EXPECT_FALSE(foo_copy.HasPrefix("ba"));
  EXPECT_FALSE(foo_copy.HasPrefix("b"));

  EXPECT_EQ(0, foo_copy.LexicographicalCompare(foo));
  EXPECT_EQ(0, bar_copy.LexicographicalCompare(bar));
  EXPECT_GT(foo_copy.LexicographicalCompare(bar_copy), 0);
  EXPECT_LT(bar_copy.LexicographicalCompare(foo_copy), 0);

  std::string new_foo;
  new_foo += 'f';
  new_foo += 'o';
  new_foo += 'o';
  Chunk foo_from_std_string(new_foo);

  EXPECT_FALSE(foo_from_std_string.empty());
  EXPECT_EQ(3u, foo_from_std_string.length());
  EXPECT_EQ(0, ::strcmp("foo", foo_from_std_string.c_str()));

  EXPECT_EQ(0, ::strcmp(foo_copy.c_str(), foo_from_std_string.c_str()));
  EXPECT_FALSE(foo_copy.c_str() == foo_from_std_string.c_str());

  ChunkDB db;

  UniqueChunk unique_foo_1 = db[foo];
  UniqueChunk unique_foo_2 = db[foo_copy];
  UniqueChunk unique_foo_3 = db[foo_from_std_string];
  EXPECT_EQ(unique_foo_1.c_str(), foo.c_str());
  EXPECT_EQ(unique_foo_2.c_str(), foo.c_str());
  EXPECT_EQ(unique_foo_3.c_str(), foo.c_str());
  EXPECT_TRUE(unique_foo_1 == unique_foo_2);
  EXPECT_TRUE(unique_foo_2 == unique_foo_3);
  EXPECT_FALSE(unique_foo_1 != unique_foo_3);
  EXPECT_FALSE(unique_foo_2 != unique_foo_1);
  EXPECT_FALSE(unique_foo_3 != unique_foo_2);
  EXPECT_FALSE(unique_foo_1 < unique_foo_2);
  EXPECT_FALSE(unique_foo_2 > unique_foo_3);
  EXPECT_TRUE(unique_foo_1 <= unique_foo_2);
  EXPECT_TRUE(unique_foo_2 >= unique_foo_3);
  EXPECT_FALSE(unique_foo_1 != unique_foo_2);

  UniqueChunk unique_bar_1 = db[bar];
  UniqueChunk unique_bar_2 = db[bar_copy];
  EXPECT_EQ(unique_bar_1.c_str(), bar.c_str());
  EXPECT_EQ(unique_bar_2.c_str(), bar.c_str());
  EXPECT_TRUE(unique_bar_1 == unique_bar_2);
  EXPECT_FALSE(unique_bar_1 != unique_bar_2);

  EXPECT_TRUE(unique_foo_1 != unique_bar_1);
  EXPECT_FALSE(unique_foo_1 == unique_bar_1);

  const bool dir = (unique_foo_1 < unique_bar_1);  // Can be either way.
  EXPECT_EQ(dir, unique_foo_1 <= unique_bar_1);
  EXPECT_EQ(!dir, unique_foo_1 > unique_bar_1);
  EXPECT_EQ(!dir, unique_foo_1 >= unique_bar_1);

  const char* pchar_meh_more_stuff = "meh\0more\0good stuff";
  const Chunk meh_1 = Chunk("meh", 3);
  const Chunk meh_2 = Chunk(pchar_meh_more_stuff, 3);
  EXPECT_EQ(0, meh_1.LexicographicalCompare(meh_2));
  EXPECT_EQ(0, meh_2.LexicographicalCompare(meh_1));

  UniqueChunk unique_meh_1 = db.FromConstChunk(meh_1);
  UniqueChunk unique_meh_2 = db.FromConstChunk(meh_2);
  EXPECT_TRUE(unique_meh_1 == unique_meh_2);

  const Chunk meh_more_1 = Chunk("meh\0more\0stuff", 8);
  const Chunk meh_more_2 = Chunk(pchar_meh_more_stuff, 8);
  EXPECT_EQ(0, meh_more_1.LexicographicalCompare(meh_more_2));
  EXPECT_EQ(0, meh_more_2.LexicographicalCompare(meh_more_1));

  EXPECT_EQ(-1, meh_1.LexicographicalCompare(meh_more_1));

  UniqueChunk unique_meh_more_1 = db.FromConstChunk(meh_more_1);
  UniqueChunk unique_meh_more_2 = db.FromConstChunk(meh_more_2);
  EXPECT_TRUE(unique_meh_more_1 == unique_meh_more_2);

  EXPECT_FALSE(unique_meh_1 == unique_meh_more_1);
  EXPECT_FALSE(unique_meh_1 == unique_meh_more_2);
  EXPECT_FALSE(unique_meh_2 == unique_meh_more_1);
  EXPECT_FALSE(unique_meh_2 == unique_meh_more_2);
}
