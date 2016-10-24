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

#include "dflags.h"

#include "../../3rdparty/gtest/gtest-main.h"

#include <sstream>
#include <string>

TEST(DFlags, DefinesAFlag) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_int8(foo, 42, "");
  DEFINE_uint8(bar, 42, "");
  static_assert(std::is_same<decltype(FLAGS_foo), int8_t>::value, "");
  static_assert(std::is_same<decltype(FLAGS_bar), uint8_t>::value, "");
  EXPECT_EQ(42, FLAGS_foo);
  EXPECT_EQ(42, FLAGS_bar);
  FLAGS_foo = -1;
  FLAGS_bar = -1;
  EXPECT_EQ(-1, FLAGS_foo);
  EXPECT_EQ(255, FLAGS_bar);
  EXPECT_NE(255, FLAGS_foo);
  EXPECT_NE(-1, FLAGS_bar);
}

TEST(DFlags, ParsesAFlagUsingSingleDash) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_int16(foo, 1, "");
  static_assert(std::is_same<decltype(FLAGS_foo), int16_t>::value, "");
  EXPECT_EQ(1, FLAGS_foo);
  int argc = 3;
  char p1[] = "./ParsesAFlagUsingSingleDash";
  char p2[] = "-foo";
  char p3[] = "2";
  char* pp[] = {p1, p2, p3};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ(2, FLAGS_foo);
  ASSERT_EQ(1, argc);
  EXPECT_EQ("./ParsesAFlagUsingSingleDash", std::string(argv[0]));
}

TEST(DFlags, ParsesAFlagUsingDoubleDash) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_uint16(bar, 1, "");
  static_assert(std::is_same<decltype(FLAGS_bar), uint16_t>::value, "");
  EXPECT_EQ(1, FLAGS_bar);
  int argc = 3;
  char p1[] = "./ParsesAFlagUsingDoubleDash";
  char p2[] = "--bar";
  char p3[] = "2";
  char* pp[] = {p1, p2, p3};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ(2, FLAGS_bar);
  ASSERT_EQ(1, argc);
  EXPECT_EQ("./ParsesAFlagUsingDoubleDash", std::string(argv[0]));
}

TEST(DFlags, ParsesAFlagUsingSingleDashEquals) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_int32(baz, 1, "");
  static_assert(std::is_same<decltype(FLAGS_baz), int32_t>::value, "");
  EXPECT_EQ(1, FLAGS_baz);
  int argc = 2;
  char p1[] = "./ParsesAFlagUsingSingleDashEquals";
  char p2[] = "-baz=2";
  char* pp[] = {p1, p2};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ(2, FLAGS_baz);
  ASSERT_EQ(1, argc);
  EXPECT_EQ("./ParsesAFlagUsingSingleDashEquals", std::string(argv[0]));
}

TEST(DFlags, ParsesAFlagUsingDoubleDashEquals) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_uint32(meh, 1, "");
  static_assert(std::is_same<decltype(FLAGS_meh), uint32_t>::value, "");
  EXPECT_EQ(1u, FLAGS_meh);
  int argc = 2;
  char p1[] = "./ParsesAFlagUsingDoubleDashEquals";
  char p2[] = "--meh=2";
  char* pp[] = {p1, p2};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ(2u, FLAGS_meh);
  ASSERT_EQ(1, argc);
  EXPECT_EQ("./ParsesAFlagUsingDoubleDashEquals", std::string(argv[0]));
}

TEST(DFlags, MultipleCallsToParseDFlagsDeathTest) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  int argc = 1;
  char p1[] = "./MultipleCallsToParseDFlagsDeathTest";
  char* pp[] = {p1};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  ASSERT_DEATH(ParseDFlags(&argc, &argv), "ParseDFlags\\(\\) is called more than once\\.");
}

TEST(DFlags, ParsesMultipleFlags) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_string(flag_string, "", "");
  DEFINE_bool(flag_bool1, false, "");
  DEFINE_bool(flag_bool2, true, "");
  DEFINE_int64(flag_int64, 0, "");
  DEFINE_uint64(flag_uint64, 0u, "");
  DEFINE_size_t(flag_size_t, 0u, "");
  static_assert(std::is_same<decltype(FLAGS_flag_string), std::string>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_bool1), bool>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_bool2), bool>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_int64), int64_t>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_uint64), uint64_t>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_size_t), size_t>::value, "");
  EXPECT_EQ("", FLAGS_flag_string);
  EXPECT_FALSE(FLAGS_flag_bool1);
  EXPECT_TRUE(FLAGS_flag_bool2);
  EXPECT_EQ(0, FLAGS_flag_int64);
  EXPECT_EQ(0ull, FLAGS_flag_uint64);
  EXPECT_EQ(0u, FLAGS_flag_size_t);
  int argc = 9;
  char p1[] = "./ParsesMultipleFlags";
  char p2[] = "-flag_string";
  char p3[] = "foo bar";
  char p4[] = "-flag_bool1=true";
  char p5[] = "-flag_bool2=false";
  char p6[] = "--flag_int64=1000000000000000000";
  char p7[] = "--flag_uint64=4000000000000000000";
  char p8[] = "--flag_size_t=42";
  char p9[] = "remaining_parameter";
  char* pp[] = {p1, p2, p3, p4, p5, p6, p7, p8, p9};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ("foo bar", FLAGS_flag_string);
  EXPECT_TRUE(FLAGS_flag_bool1);
  EXPECT_FALSE(FLAGS_flag_bool2);
  EXPECT_EQ(static_cast<int64_t>(1e18), FLAGS_flag_int64);
  EXPECT_EQ(static_cast<uint64_t>(4e18), FLAGS_flag_uint64);
  EXPECT_EQ(42u, FLAGS_flag_size_t);
  ASSERT_EQ(2, argc);
  EXPECT_EQ("./ParsesMultipleFlags", std::string(argv[0]));
  EXPECT_EQ("remaining_parameter", std::string(argv[1]));
}

TEST(DFlags, ParsesEmptyString) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_string(empty_string, "not yet", "");
  static_assert(std::is_same<decltype(FLAGS_empty_string), std::string>::value, "");
  EXPECT_EQ("not yet", FLAGS_empty_string);
  int argc = 3;
  char p1[] = "./ParsesEmptyString";
  char p2[] = "--empty_string";
  char p3[] = "";
  char* pp[] = {p1, p2, p3};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ("", FLAGS_empty_string);
}

TEST(DFlags, ParsesEmptyStringUsingEquals) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_string(empty_string, "not yet", "");
  static_assert(std::is_same<decltype(FLAGS_empty_string), std::string>::value, "");
  EXPECT_EQ("not yet", FLAGS_empty_string);
  int argc = 2;
  char p1[] = "./ParsesEmptyStringUsingEquals";
  char p2[] = "--empty_string=";
  char* pp[] = {p1, p2};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ("", FLAGS_empty_string);
}

TEST(DFlags, NoValueAllowedForBooleanFlag) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_bool(random_bool, false, "");
  int argc = 2;
  char p1[] = "./NoValueDeathTest";
  char p2[] = "--random_bool";
  char* pp[] = {p1, p2};
  char** argv = pp;
  EXPECT_FALSE(FLAGS_random_bool);
  ParseDFlags(&argc, &argv);
  EXPECT_TRUE(FLAGS_random_bool);
}

TEST(DFlags, ParsingContinuesAfterNoValueForBooleanFlag) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_bool(another_random_bool, false, "");
  DEFINE_string(foo, "", "");
  int argc = 4;
  char p1[] = "./NoValueDeathTest";
  char p2[] = "--another_random_bool";
  char p3[] = "--foo";
  char p4[] = "bar";
  char* pp[] = {p1, p2, p3, p4};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_TRUE(FLAGS_another_random_bool);
  EXPECT_EQ("bar", FLAGS_foo);
  EXPECT_EQ(1, argc);  // Double-check all the arguments have been parsed.
}

TEST(DFlags, BooleanFlagCanHaveAValueToo) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_bool(one_more_random_bool, true, "");
  DEFINE_string(bar, "", "");
  int argc = 5;
  char p1[] = "./NoValueDeathTest";
  char p2[] = "--one_more_random_bool";
  char p3[] = "false";
  char p4[] = "--bar";
  char p5[] = "baz";
  char* pp[] = {p1, p2, p3, p4, p5};
  char** argv = pp;
  EXPECT_TRUE(FLAGS_one_more_random_bool);
  ParseDFlags(&argc, &argv);
  EXPECT_FALSE(FLAGS_one_more_random_bool);
  EXPECT_EQ("baz", FLAGS_bar);
  EXPECT_EQ(1, argc);  // Double-check the `false` from `p3` has been parsed.
}

TEST(DFlags, PrintsHelpDeathTest) {
  struct MockCerrHelpPrinter : ::dflags::FlagsManager::DefaultRegisterer {
    virtual std::ostream& HelpPrinterOStream() const override { return std::cerr; }
    virtual int HelpPrinterReturnCode() const override { return -1; }
  };
  MockCerrHelpPrinter local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_bool(bar, true, "Bar.");
  DEFINE_string(foo, "TRUE THIS", "Foo.");
  DEFINE_int32(meh, 42, "Meh.");
  int argc = 2;
  char p1[] = "./PrintsHelpDeathTest";
  char p2[] = "--help";
  char* pp[] = {p1, p2};
  char** argv = pp;
  EXPECT_DEATH(ParseDFlags(&argc, &argv),
               "3 flags registered.\n"
               "\t--bar , bool\n"
               "\t\tBar\\.\n"
               "\t\tDefault value\\: True\n"
               "\t--foo , std\\:\\:string\n"
               "\t\tFoo\\.\n"
               "\t\tDefault value: 'TRUE THIS'\n"
               "\t--meh , int32_t\n"
               "\t\tMeh\\.\n"
               "\t\tDefault value: 42\n");
}

TEST(DFlags, UndefinedFlagDeathTest) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  {
    int argc = 3;
    char p1[] = "./UndefinedFlagDeathTest";
    char p2[] = "--undefined_flag";
    char p3[] = "100";
    char* pp[] = {p1, p2, p3};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Undefined flag: 'undefined_flag'\\.");
  }
  {
    int argc = 2;
    char p1[] = "./UndefinedFlagDeathTest";
    char p2[] = "--another_undefined_flag=5000";
    char* pp[] = {p1, p2};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Undefined flag: 'another_undefined_flag'\\.");
  }
}

TEST(DFlags, TooManyDashesDeathTest) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  int argc = 2;
  char p1[] = "./TooManyDashesDeathTest";
  char p2[] = "---whatever42";
  char* pp[] = {p1, p2};
  char** argv = pp;
  EXPECT_DEATH(ParseDFlags(&argc, &argv), "Parameter: '---whatever42' has too many dashes in front\\.");
}

TEST(DFlags, NoValueDeathTest) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_string(random_string, "", "");
  int argc = 2;
  char p1[] = "./NoValueDeathTest";
  char p2[] = "--random_string";
  char* pp[] = {p1, p2};
  char** argv = pp;
  EXPECT_DEATH(ParseDFlags(&argc, &argv), "Flag: 'random_string' is not provided with the value\\.");
}

TEST(DFlags, UnparsableValueDeathTest) {
  ::dflags::FlagsManager::DefaultRegisterer local_registerer;
  auto local_registerer_scope = ::dflags::FlagsManager::ScopedSingletonInjector(local_registerer);
  DEFINE_bool(flag_bool, false, "");
  DEFINE_int16(flag_int16, 0, "");
  {
    int argc = 2;
    char p1[] = "./UnparsableValueDeathTest";
    char p2[] = "--flag_bool=uncertain";
    char* pp[] = {p1, p2};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Can not parse 'uncertain' for flag 'flag_bool'\\.");
  }
  {
    int argc = 2;
    char p1[] = "./UnparsableValueDeathTest";
    char p2[] = "--flag_int16=987654321";
    char* pp[] = {p1, p2};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Can not parse '987654321' for flag 'flag_int16'\\.");
  }
  {
    int argc = 2;
    char p1[] = "./UnparsableValueDeathTest";
    char p2[] = "--flag_bool=";
    char* pp[] = {p1, p2};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Can not parse '' for flag 'flag_bool'\\.");
  }
  {
    int argc = 2;
    char p1[] = "./UnparsableValueDeathTest";
    char p2[] = "--flag_int16=";
    char* pp[] = {p1, p2};
    char** argv = pp;
    EXPECT_DEATH(ParseDFlags(&argc, &argv), "Can not parse '' for flag 'flag_int16'\\.");
  }
}
