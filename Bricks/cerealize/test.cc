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

// TOOD(dkorolev): WORK IN PROGRESS.
//
// TODO(dkorolev): TESTS TO WRITE.
//
// * Compare against golden files.
// * Polymorphic non-trivial hierarchy diagram, JSON and binary with golden files.
// * Parse polymorphic objects and count them: Via lambda, via class, via tuple<> type list spec.
// * Add test names to file names.
// * Add and parse --n=10000 entries: --vbros=FILENAME and --vyser=FILENAME.
// * Non-polymorphic types, JSON and binary, success and failure.

// It's probably best to put docu-based headers first.
#include "docu/docu_01cerealize_01_test.cc"
#include "docu/docu_01cerealize_02_test.cc"
#include "docu/docu_01cerealize_03_test.cc"
#include "docu/docu_01cerealize_04_test.cc"

#include <iostream>
#include <tuple>

#include "cerealize.h"

#include "test/test_event_base.h"
#include "test/test_event_derived.cc.h"

#include "../file/file.h"

#include "../strings/printf.h"

#include "../dflags/dflags.h"
#include "../3party/gtest/gtest-main-with-dflags.h"

using namespace bricks;
using namespace cerealize;

using strings::Printf;

DEFINE_string(cerealize_test_tmpdir, ".noshit", "The directory to create temporary files in.");

// TODO(dkorolev): Move this helper into 3party/gtest/ ?
static std::string CurrentTestName() {
  // via https://code.google.com/p/googletest/wiki/AdvancedGuide#Getting_the_Current_Test%27s_Name
  return ::testing::UnitTest::GetInstance()->current_test_info()->name();
}

static std::string CurrentTestTempFileName() {
  return FileSystem::JoinPath(FLAGS_cerealize_test_tmpdir, CurrentTestName());
}

struct No {};
struct Yes {
  template <class A>
  void serialize(A&) {}
};

TEST(Cerealize, CompileTimeTests) {
  static_assert(!is_cerealizable<No>::value, "");
  static_assert(is_cerealizable<Yes>::value, "");

  static_assert(!is_string_type<int>::value, "");

  static_assert(is_string_type<char>::value, "");

  static_assert(is_string_type<char*>::value, "");

  static_assert(is_string_type<const char*>::value, "");
  static_assert(is_string_type<const char*&>::value, "");
  static_assert(is_string_type<const char*&&>::value, "");
  static_assert(is_string_type<char*&&>::value, "");

  static_assert(is_string_type<std::string>::value, "");
  static_assert(is_string_type<const std::string&>::value, "");
  static_assert(is_string_type<std::string&&>::value, "");

  static_assert(is_string_type<std::vector<char>>::value, "");
  static_assert(is_string_type<const std::vector<char>&>::value, "");
  static_assert(is_string_type<std::vector<char>&&>::value, "");

  static_assert(!is_write_cerealizable<const char*>::value, "");
  static_assert(!is_write_cerealizable<const char*&>::value, "");
  static_assert(!is_write_cerealizable<const char*&&>::value, "");

  static_assert(!is_write_cerealizable<char*>::value, "");
  static_assert(!is_write_cerealizable<char*&>::value, "");
  static_assert(!is_write_cerealizable<char*&&>::value, "");

  static_assert(!is_write_cerealizable<std::vector<char>>::value, "");
  static_assert(!is_write_cerealizable<const std::vector<char>&>::value, "");
  static_assert(!is_write_cerealizable<std::vector<char>&&>::value, "");
}

struct CerealTestObject {
  int number;
  size_t unsigned_number;
  std::string text;
  std::vector<int> array;  // Visual C++ does not support the `= { 1, 2, 3 };` non-static member initialization.
  CerealTestObject() : number(42), unsigned_number(100), text("text"), array({1, 2, 3}) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(number), CEREAL_NVP(unsigned_number), CEREAL_NVP(text), CEREAL_NVP(array));
  }
};

TEST(Cerealize, JSON) {
  EXPECT_EQ("{\"value0\":{\"number\":42,\"unsigned_number\":100,\"text\":\"text\",\"array\":[1,2,3]}}",
            JSON(CerealTestObject()));
}

TEST(Cerealize, NamedJSON) {
  EXPECT_EQ("{\"BAZINGA\":{\"number\":42,\"unsigned_number\":100,\"text\":\"text\",\"array\":[1,2,3]}}",
            JSON(CerealTestObject(), "BAZINGA"));
}

TEST(Cerealize, BinarySerializesAndParses) {
  FileSystem::MkDir(FLAGS_cerealize_test_tmpdir, FileSystem::MkDirParameters::Silent);
  FileSystem::RmFile(CurrentTestTempFileName(), FileSystem::RmFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  {
    CerealFileAppender<CerealFormat::Binary> f1(CurrentTestTempFileName());
    f1 << a << b;
    EXPECT_EQ(2u, f1.EntriesAppended());
    EXPECT_EQ(115u, f1.BytesAppended());
  }
  {
    CerealFileAppender<CerealFormat::Binary> f2(CurrentTestTempFileName());
    f2 << c;
    EXPECT_EQ(1u, f2.EntriesAppended());
    EXPECT_EQ(58u, f2.BytesAppended());
  }

  CerealFileParser<MapsYouEventBase, CerealFormat::Binary> f(CurrentTestTempFileName());

  std::ostringstream os;
  while (f.Next([&os](const MapsYouEventBase& e) { e.AppendTo(os) << '\n'; }))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo\n"
      "Type=EventAppSuspend, ShortType=\"as\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar\n"
      "Type=EventAppResume, ShortType=\"ar\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz\n",
      os.str());
}

TEST(Cerealize, JSONSerializesAndParses) {
  FileSystem::MkDir(FLAGS_cerealize_test_tmpdir, FileSystem::MkDirParameters::Silent);
  FileSystem::RmFile(CurrentTestTempFileName(), FileSystem::RmFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  {
    CerealFileAppender<CerealFormat::JSON> f1(CurrentTestTempFileName());
    f1 << a;
    EXPECT_EQ(1u, f1.EntriesAppended());
    EXPECT_EQ(169u, f1.BytesAppended());
  }
  {
    CerealFileAppender<CerealFormat::JSON> f2(CurrentTestTempFileName());
    f2 << b << c;
    EXPECT_EQ(2u, f2.EntriesAppended());
    EXPECT_EQ(340u, f2.BytesAppended());
    EXPECT_EQ(509u, f2.TotalFileSize());
  }

  CerealFileParser<MapsYouEventBase, CerealFormat::JSON> f(CurrentTestTempFileName());

  std::ostringstream os;
  while (f.Next([&os](const MapsYouEventBase& e) { e.AppendTo(os) << '\n'; }))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo\n"
      "Type=EventAppSuspend, ShortType=\"as\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar\n"
      "Type=EventAppResume, ShortType=\"ar\", UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz\n",
      os.str());
}

TEST(Cerealize, ConsumerSupportsPolymorphicTypes) {
  FileSystem::MkDir(FLAGS_cerealize_test_tmpdir, FileSystem::MkDirParameters::Silent);
  FileSystem::RmFile(CurrentTestTempFileName(), FileSystem::RmFileParameters::Silent);

  EventAppStart a;
  EventAppSuspend b;
  EventAppResume c;

  CerealFileAppender<CerealFormat::Binary>(CurrentTestTempFileName()) << a << b;
  CerealFileAppender<CerealFormat::Binary>(CurrentTestTempFileName()) << c;

  CerealFileParser<MapsYouEventBase, CerealFormat::Binary> f(CurrentTestTempFileName());

  struct ExampleConsumer {
    // TODO(dkorolev): Chat with Alex what the best way to handle this would be.
    // I looked through Cereal, they use a map<k,v> using typeid. It's a bit off what we need here.
    typedef MapsYouEventBase BASE_TYPE;
    // Note that this tuple does not list `EventAppSuspend`.
    typedef std::tuple<EventAppStart, EventAppResume> DERIVED_TYPE_LIST;
    enum FixTypedefDefinedButNotUsedWarning { FOO = sizeof(BASE_TYPE), BAR = sizeof(DERIVED_TYPE_LIST) };

    std::ostringstream os;
    void operator()(const MapsYouEventBase& e) { e.AppendTo(os) << " *** WRONG *** " << std::endl; }
    void operator()(const EventAppStart& e) { e.AppendTo(os) << " START " << std::endl; }
    void operator()(const EventAppResume& e) { e.AppendTo(os) << " RESUME " << std::endl; }

    // This function will not be called since the `EventAppSuspend` type is not in `DERIVED_TYPE_LIST` above.
    void operator()(const EventAppSuspend& e) { e.AppendTo(os) << " *** NOT CALLED SUSPEND *** " << std::endl; }

    ExampleConsumer() = default;
    ExampleConsumer(const ExampleConsumer& rhs) = delete;
  };

  ExampleConsumer consumer;
  while (f.NextWithDispatching<ExampleConsumer>(consumer))
    ;

  EXPECT_EQ(
      "Type=EventAppStart, ShortType=\"a\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, foo=foo START \n"
      "Type=EventAppSuspend, ShortType=\"as\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, bar=bar *** WRONG *** \n"
      "Type=EventAppResume, ShortType=\"ar\","
      " UID=, UID_Google=, UID_Apple=, UID_Facebook=, baz=baz RESUME \n",
      consumer.os.str());
}

// CT stands for CerealizeTest.
// Global symbol names should be unique since all the tests are compiled together for the coverage report.
struct CTBase {
  int number = 0;
  template <class A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(number));
  }
  virtual std::string AsString() const = 0;
};

struct CTDerived1 : CTBase {
  std::string foo;
  template <class A>
  void serialize(A& ar) {
    CTBase::serialize(ar);
    ar(CEREAL_NVP(foo));
  }
  virtual std::string AsString() const override { return Printf("Derived1(%d,'%s')", number, foo.c_str()); }
};
CEREAL_REGISTER_TYPE(CTDerived1);

struct CTDerived2 : CTBase {
  std::string bar;
  template <class A>
  void serialize(A& ar) {
    CTBase::serialize(ar);
    ar(CEREAL_NVP(bar));
  }
  virtual std::string AsString() const override { return Printf("Derived2(%d,'%s')", number, bar.c_str()); }
  void FromInvalidJSON(const std::string& input_json) {
    number = -1;
    bar = "Invalid JSON: " + input_json;
  }
};
CEREAL_REGISTER_TYPE(CTDerived2);

static_assert(!HasFromInvalidJSON<CTDerived1>::value, "");
static_assert(HasFromInvalidJSON<CTDerived2>::value, "");

TEST(Cerealize, JSONStringifyWithBase) {
  CTDerived1 d1;
  d1.foo = "fffuuuuu";
  EXPECT_EQ("{\"value0\":{\"number\":0,\"foo\":\"fffuuuuu\"}}", JSON(d1));
  EXPECT_EQ(
      "{\"value0\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"CTDerived1\","
      "\"ptr_wrapper\":{\"valid\":1,\"data\":{\"number\":0,\"foo\":\"fffuuuuu\"}}}}",
      JSON(WithBaseType<CTBase>(d1)));

  CTDerived2 d2;
  d2.bar = "bwahaha";
  EXPECT_EQ("{\"value0\":{\"number\":0,\"bar\":\"bwahaha\"}}", JSON(d2));
  EXPECT_EQ(
      "{\"value0\":{\"polymorphic_id\":2147483649,\"polymorphic_name\":\"CTDerived2\","
      "\"ptr_wrapper\":{\"valid\":1,\"data\":{\"number\":0,\"bar\":\"bwahaha\"}}}}",
      JSON(WithBaseType<CTBase>(d2)));
}

TEST(Cerealize, ParseJSONReturnValueSyntax) {
  CTDerived1 input;
  input.number = 42;
  input.foo = "string";
  CTDerived1 output = ParseJSON<CTDerived1>(JSON(input));
  EXPECT_EQ(42, output.number);
  EXPECT_EQ("string", output.foo);
}

TEST(Cerealize, ParseJSONReferenceSyntax) {
  CTDerived1 input;
  input.number = 42;
  input.foo = "string";
  CTDerived1 output;
  ParseJSON(JSON(input), output);
  EXPECT_EQ(42, output.number);
  EXPECT_EQ("string", output.foo);
}

TEST(Cerealize, ParseJSONSupportsPolymorphicTypes) {
  CTDerived1 d1;
  d1.number = 1;
  d1.foo = "foo";
  CTDerived2 d2;
  d2.number = 2;
  d2.bar = "bar";

  {
    EXPECT_EQ("Derived1(1,'foo')",
              ParseJSON<std::unique_ptr<CTBase>>(JSON(WithBaseType<CTBase>(d1)))->AsString());
    EXPECT_EQ("Derived2(2,'bar')",
              ParseJSON<std::unique_ptr<CTBase>>(JSON(WithBaseType<CTBase>(d2)))->AsString());
  }

  {
    std::unique_ptr<CTBase> placeholder;
    EXPECT_EQ("Derived1(1,'foo')", ParseJSON(JSON(WithBaseType<CTBase>(d1)), placeholder)->AsString());
    EXPECT_EQ("Derived2(2,'bar')", ParseJSON(JSON(WithBaseType<CTBase>(d2)), placeholder)->AsString());
  }
}

TEST(Cerealize, ParseJSONThrowsOnError) {
  ASSERT_THROW(ParseJSON<CTDerived1>("surely not a valid JSON"), ParseJSONException);
}

TEST(Cerealize, ParseJSONErrorCanBeMadeNonThrowing) {
  EXPECT_EQ("Derived2(-1,'Invalid JSON: BAZINGA')", ParseJSON<CTDerived2>("BAZINGA").AsString());
}

TEST(Cerealize, Base64Encode) {
  EXPECT_EQ("MTIzNDU=", Base64Encode("12345"));
  EXPECT_EQ("NzY1NFh5Wg==", Base64Encode("7654XyZ"));
}
