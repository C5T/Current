#include "dflags.h"

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

#include <string>
#include <sstream>

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
  EXPECT_EQ(1, FLAGS_meh);
  int argc = 2;
  char p1[] = "./ParsesAFlagUsingDoubleDashEquals";
  char p2[] = "--meh=2";
  char* pp[] = {p1, p2};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ(2, FLAGS_meh);
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
  DEFINE_uint64(flag_uint64, 0, "");
  static_assert(std::is_same<decltype(FLAGS_flag_string), std::string>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_bool1), bool>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_bool2), bool>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_int64), int64_t>::value, "");
  static_assert(std::is_same<decltype(FLAGS_flag_uint64), uint64_t>::value, "");
  EXPECT_EQ("", FLAGS_flag_string);
  EXPECT_FALSE(FLAGS_flag_bool1);
  EXPECT_TRUE(FLAGS_flag_bool2);
  EXPECT_EQ(0, FLAGS_flag_int64);
  EXPECT_EQ(0, FLAGS_flag_uint64);
  int argc = 8;
  char p1[] = "./ParsesMultipleFlags";
  char p2[] = "-flag_string";
  char p3[] = "foo bar";
  char p4[] = "-flag_bool1=true";
  char p5[] = "-flag_bool2=false";
  char p6[] = "--flag_int64=1000000000000000000";
  char p7[] = "--flag_uint64=4000000000000000000";
  char p8[] = "remaining_parameter";
  char* pp[] = {p1, p2, p3, p4, p5, p6, p7, p8};
  char** argv = pp;
  ParseDFlags(&argc, &argv);
  EXPECT_EQ("foo bar", FLAGS_flag_string);
  EXPECT_TRUE(FLAGS_flag_bool1);
  EXPECT_FALSE(FLAGS_flag_bool2);
  EXPECT_EQ(static_cast<int64_t>(1e18), FLAGS_flag_int64);
  EXPECT_EQ(static_cast<uint64_t>(4e18), FLAGS_flag_uint64);
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

TEST(DFlags, PrintsHelpDeathTest) {
  struct MockCerrHelpPrinter : ::dflags::FlagsManager::DefaultRegisterer {
    virtual std::ostream& HelpPrinterOStream() const override {
      return std::cerr;
    }
    virtual int HelpPrinterReturnCode() const override {
      return -1;
    }
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
  int argc = 2;
  char p1[] = "./NoValueDeathTest";
  char p2[] = "--whatever";
  char* pp[] = {p1, p2};
  char** argv = pp;
  EXPECT_DEATH(ParseDFlags(&argc, &argv), "Flag: 'whatever' is not provided with the value\\.");
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
