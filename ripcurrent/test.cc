/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../port.h"

#include <atomic>

#include "ripcurrent.h"

#include "../bricks/dflags/dflags.h"

#include "../bricks/strings/join.h"
#include "../bricks/strings/split.h"

#include "../blocks/http/api.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_uint16(ripcurrent_http_test_port, PickPortForUnitTest(), "Local port to use for RipCurrent unit test.");

// `clang-format` messes up macro-defined class definitions, so disable it temporarily for this section. -- D.K.

// clang-format off
namespace ripcurrent_unittest {

struct LifeUniverseAndEverything {};

CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(value, int32_t);
  CURRENT_CONSTRUCTOR(Integer)(int32_t value = 0) : value(value) {}
  CURRENT_CONSTRUCTOR(Integer)(LifeUniverseAndEverything) : value(42) {}
};

// `RCEmit`: The emitter of events. Emits the integers passed to its constructor.
RIPCURRENT_NODE(RCEmit, void, Integer) {
  static std::string UnitTestClassName() { return "RCEmit"; }
  RCEmit() {}  // LCOV_EXCL_LINE
  RCEmit(int a) { emit<Integer>(a); }
  RCEmit(int a, int b) {
    emit<Integer>(a);
    emit<Integer>(b);
  }
  RCEmit(int a, int b, int c) {
    emit<Integer>(a);
    emit<Integer>(b);
    emit<Integer>(c);
  }
};
#define RCEmit(...) RIPCURRENT_MACRO(RCEmit, __VA_ARGS__)

// `RCMult`: The processor of events. Multiplies each integer by what was passed to its constructor.
RIPCURRENT_NODE(RCMult, Integer, Integer) {
  static std::string UnitTestClassName() { return "RCMult"; }
  int k;
  RCMult(int k = 1) : k(k) {}
  void f(Integer x) { emit<Integer>(x.value * k); }
};
#define RCMult(...) RIPCURRENT_MACRO(RCMult, __VA_ARGS__)

// `RCDump`: The destination of events. Collects the output integers.
RIPCURRENT_NODE(RCDump, Integer, void) {
  static std::string UnitTestClassName() { return "RCDump"; }
  std::vector<int>* ptr;
  std::atomic_size_t* cnt = nullptr;
  RCDump() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  RCDump(std::vector<int>& ref) : ptr(&ref) {}
  RCDump(std::vector<int>& ref, std::atomic_size_t& cnt) : ptr(&ref), cnt(&cnt) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(x.value);
    if (cnt) {
      ++(*cnt);
    }
  }
};
#define RCDump(...) RIPCURRENT_MACRO(RCDump, __VA_ARGS__)

}  // namespace ripcurrent_unittest
// clang-format on

TEST(RipCurrent, TypeStaticAsserts) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  using rcemit_t = RIPCURRENT_UNDERLYING_TYPE(RCEmit());
  using rcdump_t = RIPCURRENT_UNDERLYING_TYPE(RCDump(std::ref(result)));

  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::LHSTypes<>, typename rcemit_t::input_t>), "");
  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::RHSTypes<Integer>, typename rcemit_t::output_t>),
                "");

  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::LHSTypes<Integer>, typename rcdump_t::input_t>),
                "");
  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::RHSTypes<>, typename rcdump_t::output_t>), "");

  static_assert(
      sizeof(is_same_or_compile_error<decltype(RCEmit(1, 2, 3)),
                                      current::ripcurrent::UserCodeImpl<current::ripcurrent::LHSTypes<>,
                                                                        current::ripcurrent::RHSTypes<Integer>,
                                                                        rcemit_t>>),
      "");

  static_assert(
      sizeof(is_same_or_compile_error<decltype(RCDump(std::ref(result))),
                                      current::ripcurrent::UserCodeImpl<current::ripcurrent::LHSTypes<Integer>,
                                                                        current::ripcurrent::RHSTypes<>,
                                                                        rcdump_t>>),
      "");
}

TEST(RipCurrent, SingleEdgeFlow) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;

  std::vector<int> result;

  (RCEmit(1, 2, 3) | RCDump(std::ref(result))).RipCurrent().Join();
  EXPECT_EQ("1,2,3", current::strings::Join(result, ','));
}

TEST(RipCurrent, SingleChainFlow) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;

  std::vector<int> result;

  (RCEmit(1, 2, 3) | RCMult(2) | RCMult(5) | RCMult(10) | RCDump(std::ref(result))).RipCurrent().Join();
  EXPECT_EQ("100,200,300", current::strings::Join(result, ','));
}

TEST(RipCurrent, DeclarationDoesNotRunConstructors) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  current::ripcurrent::LHS<current::ripcurrent::RHSTypes<Integer>> emit = RCEmit(42);
  current::ripcurrent::RHS<current::ripcurrent::LHSTypes<Integer>> dump = RCDump(std::ref(result));
  current::ripcurrent::E2E emit_dump = emit | dump;

  EXPECT_EQ("RCEmit(42) | ...", emit.Describe());
  EXPECT_EQ("... | RCDump(std::ref(result))", dump.Describe());
  EXPECT_EQ("RCEmit(42) | RCDump(std::ref(result))", emit_dump.Describe());

  EXPECT_EQ("", current::strings::Join(result, ','));
  emit_dump.RipCurrent().Join();
  EXPECT_EQ("42", current::strings::Join(result, ','));
}

TEST(RipCurrent, OrderDoesNotMatter) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  const auto a = RCEmit(1);
  const auto b = RCMult(2);
  const auto c = RCDump(std::ref(result));

  result.clear();
  (a | b | b | c).RipCurrent().Join();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  ((a | b) | (b | c)).RipCurrent().Join();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  ((a | (b | b)) | c).RipCurrent().Join();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  (a | ((b | b) | c)).RipCurrent().Join();
  EXPECT_EQ("4", current::strings::Join(result, ','));
}

TEST(RipCurrent, BuildingBlocksCanBeReused) {
  using namespace ripcurrent_unittest;

  std::vector<int> result1;
  std::vector<int> result2;

  current::ripcurrent::LHS<current::ripcurrent::RHSTypes<Integer>> emit1 = RCEmit(1);
  current::ripcurrent::LHS<current::ripcurrent::RHSTypes<Integer>> emit2 = RCEmit(2);
  current::ripcurrent::VIA<current::ripcurrent::LHSTypes<Integer>, current::ripcurrent::RHSTypes<Integer>> mult10 =
      RCMult(10);
  current::ripcurrent::RHS<current::ripcurrent::LHSTypes<Integer>> dump1 = RCDump(std::ref(result1));
  current::ripcurrent::RHS<current::ripcurrent::LHSTypes<Integer>> dump2 = RCDump(std::ref(result2));

  (emit1 | mult10 | dump1).RipCurrent().Join();
  (emit2 | mult10 | dump2).RipCurrent().Join();
  EXPECT_EQ("10", current::strings::Join(result1, ','));
  EXPECT_EQ("20", current::strings::Join(result2, ','));

  result1.clear();
  result2.clear();
  ((emit1 | mult10) | dump1).RipCurrent().Join();
  ((emit2 | mult10) | dump2).RipCurrent().Join();
  EXPECT_EQ("10", current::strings::Join(result1, ','));
  EXPECT_EQ("20", current::strings::Join(result2, ','));
}

TEST(RipCurrent, SynopsisAndDecorators) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCEmit() | ...", (RCEmit()).Describe());
  EXPECT_EQ("... | RCMult() | ...", (RCMult()).Describe());
  EXPECT_EQ("... | RCDump()", (RCDump()).Describe());

  EXPECT_EQ("RCEmit() | RCDump()", (RCEmit() | RCDump()).Describe());
  EXPECT_EQ("RCEmit() | RCMult() | RCMult() | RCMult() | RCDump()",
            (RCEmit() | RCMult() | RCMult() | RCMult() | RCDump()).Describe());

  EXPECT_EQ("RCEmit() | RCMult() | ...", (RCEmit() | RCMult()).Describe());
  EXPECT_EQ("RCEmit() | RCMult() | RCMult() | RCMult() | ...", (RCEmit() | RCMult() | RCMult() | RCMult()).Describe());

  EXPECT_EQ("... | RCMult() | RCDump()", (RCMult() | RCDump()).Describe());
  EXPECT_EQ("... | RCMult() | RCMult() | RCMult() | RCDump()", (RCMult() | RCMult() | RCMult() | RCDump()).Describe());

  EXPECT_EQ("... | RCMult() | RCMult() | RCMult() | ...", (RCMult() | RCMult() | RCMult()).Describe());

  const int blah = 5;
  EXPECT_EQ("RCEmit(1) | RCMult(2) | RCMult(3 + 4) | RCMult(blah) | RCDump()",
            (RCEmit(1) | RCMult(2) | RCMult(3 + 4) | RCMult(blah) | RCDump()).Describe());

  const int x = 1;
  const int y = 1;
  const int z = 1;
  EXPECT_EQ("RCEmit(x, y, z) | RCDump()", (RCEmit(x, y, z) | RCDump()).Describe());
}

TEST(RipCurrent, TypeIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCEmit() => { Integer } | ...", (RCEmit()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer } => RCMult() => { Integer } | ...", (RCMult()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer } => RCDump()", (RCDump()).DescribeWithTypes());
}

TEST(RipCurrent, MoreTypeSystemGuarantees) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("RCEmit", RCEmit::UnitTestClassName());
  static_assert(std::is_same_v<RCEmit::input_t, current::ripcurrent::LHSTypes<>>, "");
  static_assert(std::is_same_v<RCEmit::output_t, current::ripcurrent::RHSTypes<Integer>>, "");

  EXPECT_EQ("RCEmit", RIPCURRENT_UNDERLYING_TYPE(RCEmit())::UnitTestClassName());

  const auto emit = RCEmit();
  EXPECT_EQ("RCEmit", RIPCURRENT_UNDERLYING_TYPE(emit)::UnitTestClassName());

  EXPECT_EQ("RCMult", RCMult::UnitTestClassName());
  static_assert(std::is_same_v<RCMult::input_t, current::ripcurrent::LHSTypes<Integer>>, "");
  static_assert(std::is_same_v<RCMult::output_t, current::ripcurrent::RHSTypes<Integer>>, "");

  EXPECT_EQ("RCMult", RIPCURRENT_UNDERLYING_TYPE(RCMult())::UnitTestClassName());

  const auto mult = RCMult();
  EXPECT_EQ("RCMult", RIPCURRENT_UNDERLYING_TYPE(mult)::UnitTestClassName());

  EXPECT_EQ("RCDump", RCDump::UnitTestClassName());
  static_assert(std::is_same_v<RCDump::input_t, current::ripcurrent::LHSTypes<Integer>>, "");
  static_assert(std::is_same_v<RCDump::output_t, current::ripcurrent::RHSTypes<>>, "");

  EXPECT_EQ("RCDump", RIPCURRENT_UNDERLYING_TYPE(RCDump())::UnitTestClassName());

  const auto dump = RCDump();
  EXPECT_EQ("RCDump", RIPCURRENT_UNDERLYING_TYPE(dump)::UnitTestClassName());

  const auto emit_mult = emit | mult;
  const auto mult_dump = mult | dump;
  const auto emit_dump = emit | dump;
  const auto emit_mult_dump_1 = (emit | mult) | dump;
  const auto emit_mult_dump_2 = emit | (mult | dump);
  const auto emit_mult_mult_dump_1 = (emit | mult) | (mult | dump);
  const auto emit_mult_mult_dump_2 = emit | (mult | mult) | dump;
  const auto emit_mult_mult_dump_3 = ((emit | mult) | mult) | dump;
  const auto emit_mult_mult_dump_4 = emit | (mult | (mult | dump));

  static_assert(std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult)::input_t, current::ripcurrent::LHSTypes<>>,
                "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult)::output_t, current::ripcurrent::RHSTypes<Integer>>, "");

  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(mult_dump)::input_t, current::ripcurrent::LHSTypes<Integer>>, "");
  static_assert(std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(mult_dump)::output_t, current::ripcurrent::RHSTypes<>>,
                "");

  static_assert(std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_dump)::input_t, current::ripcurrent::LHSTypes<>>,
                "");
  static_assert(std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_dump)::output_t, current::ripcurrent::RHSTypes<>>,
                "");

  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_1)::input_t, current::ripcurrent::LHSTypes<>>, "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_1)::output_t, current::ripcurrent::RHSTypes<>>, "");

  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_2)::input_t, current::ripcurrent::LHSTypes<>>, "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_2)::output_t, current::ripcurrent::RHSTypes<>>, "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_1)::input_t, current::ripcurrent::LHSTypes<>>,
      "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_1)::output_t, current::ripcurrent::RHSTypes<>>,
      "");

  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_2)::input_t, current::ripcurrent::LHSTypes<>>,
      "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_2)::output_t, current::ripcurrent::RHSTypes<>>,
      "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_3)::input_t, current::ripcurrent::LHSTypes<>>,
      "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_3)::output_t, current::ripcurrent::RHSTypes<>>,
      "");

  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_4)::input_t, current::ripcurrent::LHSTypes<>>,
      "");
  static_assert(
      std::is_same_v<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_4)::output_t, current::ripcurrent::RHSTypes<>>,
      "");

  emit_mult.Dismiss();
  mult_dump.Dismiss();
  emit_dump.Dismiss();
  emit_mult_dump_1.Dismiss();
  emit_mult_dump_2.Dismiss();
  emit_mult_mult_dump_1.Dismiss();
  emit_mult_mult_dump_2.Dismiss();
  emit_mult_mult_dump_3.Dismiss();
  emit_mult_mult_dump_4.Dismiss();
}

// A test helper to chop off file name and line numbers from exception messages.
static std::string ExpectHasNAndReturnFirstTwoLines(size_t n, const std::string& message) {
  const std::vector<std::string> lines = current::strings::Split<current::strings::ByLines>(message);
  EXPECT_EQ(n, lines.size());
  return current::strings::Join(
      std::vector<std::string>(lines.begin(),
                               lines.begin() + std::min(static_cast<size_t>(lines.size()), static_cast<size_t>(2))),
      '\n');
}

TEST(RipCurrent, UnusedRipCurrentBlockYieldsAnException) {
  using namespace ripcurrent_unittest;

  std::string captured_error_message;
  const auto mock_scope = current::Singleton<current::ripcurrent::RipCurrentMockableErrorHandler>().ScopedInjectHandler(
      [&captured_error_message](const std::string& error_message) { captured_error_message = error_message; });

  EXPECT_EQ("", captured_error_message);
  RCEmit();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCEmit() | ...",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCMult();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCMult() | ...",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCDump();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCDump()",
      ExpectHasNAndReturnFirstTwoLines(4, captured_error_message));
  RCEmit(1) | RCMult(2);
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCEmit(1) | RCMult(2) | ...",
      ExpectHasNAndReturnFirstTwoLines(5, captured_error_message));
  RCMult(3) | RCDump();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | RCMult(3) | RCDump()",
      ExpectHasNAndReturnFirstTwoLines(5, captured_error_message));
  std::vector<int> result;
  RCEmit(42) | RCMult(100) | RCDump(std::ref(result));
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCEmit(42) | RCMult(100) | RCDump(std::ref(result))",
      ExpectHasNAndReturnFirstTwoLines(6, captured_error_message));

  { const auto tmp = RCEmit() | RCMult(1) | RCDump(std::ref(result)); }
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "RCEmit() | RCMult(1) | RCDump(std::ref(result))",
      ExpectHasNAndReturnFirstTwoLines(6, captured_error_message));

  captured_error_message = "NO ERROR";
  {
    const auto tmp = RCEmit() | RCMult(2) | RCDump(std::ref(result));
    tmp.Describe();
  }
  EXPECT_EQ("NO ERROR", captured_error_message);

  captured_error_message = "NO ERROR ONCE AGAIN";
  {
    const auto tmp = RCEmit() | RCMult(3) | RCDump(std::ref(result));
    tmp.Dismiss();
  }
  EXPECT_EQ("NO ERROR ONCE AGAIN", captured_error_message);
}

// clang-format off
namespace ripcurrent_unittest {

CURRENT_STRUCT(String) {
  CURRENT_FIELD(value, std::string);
  CURRENT_CONSTRUCTOR(String)(const std::string& value = "") : value(value) {}
  CURRENT_CONSTRUCTOR(String)(LifeUniverseAndEverything) : value("The Answer") {}
};

CURRENT_STRUCT(Bool) {
  CURRENT_FIELD(flag, bool);
  CURRENT_CONSTRUCTOR(Bool)(bool flag = false) : flag(flag) {}
};

RIPCURRENT_NODE(EmitString, (), String) {  // Note: `()` is same as `void`.
  EmitString() {
    emit<String>("Answer");
  }
};
#define EmitString(...) RIPCURRENT_MACRO(EmitString, __VA_ARGS__)

RIPCURRENT_NODE(EmitBool, (), Bool) {  // Note: `()` is same as `void`.
  EmitBool(bool value = true) {
    emit<Bool>(value);
  }
};
#define EmitBool(...) RIPCURRENT_MACRO(EmitBool, __VA_ARGS__)

RIPCURRENT_NODE(EmitInteger, (), Integer) {  // Note: Intentionally somewhat duplicates `RCEmit`. -- D.K.
  EmitInteger() {
    emit<Integer>(42);
  }
};
#define EmitInteger(...) RIPCURRENT_MACRO(EmitInteger, __VA_ARGS__)

RIPCURRENT_NODE(EmitIntegerAndString, (), (Integer, String)) {  // Note: `()` is same as `void`.
  EmitIntegerAndString() {
    emit<String>("Answer");
    emit<Integer>(42);
  }
};
#define EmitIntegerAndString(...) RIPCURRENT_MACRO(EmitIntegerAndString, __VA_ARGS__)

RIPCURRENT_NODE(MultIntegerOrString, (Integer, String), (Integer, String)) {
  const int k;
  MultIntegerOrString(int k = 101) : k(k) {}
  void f(Integer x) {
    emit<Integer>(x.value * k);
  }
  void f(String s) {
    emit<String>("Yo? " + s.value + " Yo!");
  }
};
#define MultIntegerOrString(...) RIPCURRENT_MACRO(MultIntegerOrString, __VA_ARGS__)

RIPCURRENT_NODE(DumpInteger, Integer, ()) {
  std::vector<std::string>* ptr;
  DumpInteger() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  DumpInteger(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(current::ToString(x.value));
  }
};
#define DumpInteger(...) RIPCURRENT_MACRO(DumpInteger, __VA_ARGS__)

RIPCURRENT_NODE(DumpString, String, ()) {
  std::vector<std::string>* ptr;
  DumpString() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  DumpString(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(String s) {
    CURRENT_ASSERT(ptr);
    ptr->push_back("'" + s.value + "'");
  }
};
#define DumpString(...) RIPCURRENT_MACRO(DumpString, __VA_ARGS__)

RIPCURRENT_NODE(DumpBool, Bool, ()) {
  std::vector<std::string>* ptr;
  DumpBool() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  DumpBool(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(Bool b) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(b.flag ? "True" : "False");
  }
};
#define DumpBool(...) RIPCURRENT_MACRO(DumpBool, __VA_ARGS__)

RIPCURRENT_NODE(DumpIntegerAndString, (Integer, String), ()) {
  std::vector<std::string>* ptr;
  DumpIntegerAndString() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  DumpIntegerAndString(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(current::ToString(x.value));
  }
  void f(String s) {
    CURRENT_ASSERT(ptr);
    ptr->push_back("'" + s.value + "'");
  }
};
#define DumpIntegerAndString(...) RIPCURRENT_MACRO(DumpIntegerAndString, __VA_ARGS__)

}  // namespace ripcurrent_unittest
// clang-format on

TEST(RipCurrent, CustomTypesIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("EmitIntegerAndString() => { Integer, String } | ...", (EmitIntegerAndString()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer, String } => DumpIntegerAndString()", (DumpIntegerAndString()).DescribeWithTypes());
}

TEST(RipCurrent, CustomTypesFlow) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | RIPCURRENT_DROP(Integer, String)).RipCurrent().Join();
    EXPECT_EQ("", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | RIPCURRENT_PASS(Integer, String) | RIPCURRENT_DROP(Integer, String)).RipCurrent().Join();
    EXPECT_EQ("", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | DumpIntegerAndString(std::ref(result))).RipCurrent().Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | MultIntegerOrString() | DumpIntegerAndString(std::ref(result))).RipCurrent().Join();
    EXPECT_EQ("'Yo? Answer Yo!', 4242", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | MultIntegerOrString() | MultIntegerOrString(10001) |
     DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Yo? Yo? Answer Yo! Yo!', 42424242", current::strings::Join(result, ", "));
  }
}

TEST(RipCurrent, PlusIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("EmitInteger() + EmitString() | DumpInteger() + DumpString()",
            ((EmitInteger() + EmitString()) | (DumpInteger() + DumpString())).Describe());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() | DumpInteger() + DumpString() + DumpBool()",
            ((EmitInteger() + EmitString() + EmitBool()) | (DumpInteger() + DumpString() + DumpBool())).Describe());

  EXPECT_EQ(
      "EmitInteger() + EmitString() + EmitBool() | Pass(Integer) + Pass(String) + Pass(Bool) | DumpInteger() + "
      "DumpString() + DumpBool()",
      ((EmitInteger() + EmitString() + EmitBool()) |
       (RIPCURRENT_PASS(Integer) + RIPCURRENT_PASS(String) + RIPCURRENT_PASS(Bool)) |
       (DumpInteger() + DumpString() + DumpBool())).Describe());

  EXPECT_EQ(
      "EmitInteger() + EmitString() + EmitBool() | Pass(Integer, String, Bool) | DumpInteger() + DumpString() + "
      "DumpBool()",
      ((EmitInteger() + EmitString() + EmitBool()) | RIPCURRENT_PASS(Integer, String, Bool) |
       (DumpInteger() + DumpString() + DumpBool())).Describe());

  EXPECT_EQ(
      "EmitInteger() + EmitString() + EmitBool() | Pass(Integer, String) + DumpBool() | DumpInteger() + DumpString()",
      ((EmitInteger() + EmitString() + EmitBool()) | (RIPCURRENT_PASS(Integer, String) + DumpBool()) |
       (DumpInteger() + DumpString())).Describe());

  EXPECT_EQ(
      "EmitInteger() + EmitString() + EmitBool() | DumpInteger() + Pass(String, Bool) | Pass(String) + DumpBool() | "
      "DumpString()",
      ((EmitInteger() + EmitString() + EmitBool()) | (DumpInteger() + RIPCURRENT_PASS(String, Bool)) |
       ((RIPCURRENT_PASS(String) + DumpBool()) | DumpString())).Describe());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() | DumpInteger() + Drop(String, Bool)",
            ((EmitInteger() + EmitString() + EmitBool()) | (DumpInteger() + RIPCURRENT_DROP(String, Bool))).Describe());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() | Pass(Integer) + Drop(String, Bool) | Drop(Integer)",
            ((EmitInteger() + EmitString() + EmitBool()) | (RIPCURRENT_PASS(Integer) + RIPCURRENT_DROP(String, Bool)) |
             RIPCURRENT_DROP(Integer)).Describe());

  EXPECT_EQ("EmitInteger() + EmitString() => { Integer, String } | ...",
            (EmitInteger() + EmitString()).DescribeWithTypes());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() => { Integer, String, Bool } | ...",
            (EmitInteger() + EmitString() + EmitBool()).DescribeWithTypes());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() => { Integer, String, Bool } | ...",
            ((EmitInteger() + EmitString()) + EmitBool()).DescribeWithTypes());

  EXPECT_EQ("EmitInteger() + EmitString() + EmitBool() => { Integer, String, Bool } | ...",
            (EmitInteger() + (EmitString() + EmitBool())).DescribeWithTypes());

  EXPECT_EQ("... | { Integer, String } => DumpInteger() + DumpString()",
            (DumpInteger() + DumpString()).DescribeWithTypes());
}

TEST(RipCurrent, PlusFlow) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | (RIPCURRENT_DROP(Integer) + RIPCURRENT_DROP(String))).RipCurrent().Join();
    EXPECT_EQ("", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    ((EmitInteger() + EmitString()) | DumpIntegerAndString(std::ref(result))).RipCurrent().Join();
    EXPECT_EQ("42, 'Answer'", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    ((EmitInteger() + EmitString()) | (DumpInteger(std::ref(result)) + DumpString(std::ref(result))))
        .RipCurrent()
        .Join();
    EXPECT_EQ("42, 'Answer'", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    ((EmitInteger() + EmitString() + EmitBool()) |
     (DumpInteger(std::ref(result)) + DumpString(std::ref(result)) + DumpBool(std::ref(result))))
        .RipCurrent()
        .Join();
    EXPECT_EQ("42, 'Answer', True", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    ((EmitInteger() + EmitBool(false) + EmitString() + EmitBool(true)) |
     (DumpInteger(std::ref(result)) + (DumpBool(std::ref(result)) + DumpString(std::ref(result)))))
        .RipCurrent()
        .Join();
    EXPECT_EQ("42, False, 'Answer', True", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    ((EmitInteger() + EmitBool() + EmitString()) | (RIPCURRENT_DROP(Integer, Bool) + RIPCURRENT_PASS(String)) |
     DumpString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer'", current::strings::Join(result, ", "));
  }
}

namespace ripcurrent_unittest {

// clang-format off

RIPCURRENT_NODE(ManuallyImplementedGenericPassThrough1, (Integer, String), (Integer, String)) {
  void f(Integer x) {
    emit<Integer>(x);
  }
  void f(String s) {
    emit<String>(s);
  }
};
#define ManuallyImplementedGenericPassThrough1(...) \
  RIPCURRENT_MACRO(ManuallyImplementedGenericPassThrough1, __VA_ARGS__)

RIPCURRENT_NODE(ManuallyImplementedGenericPassThrough2, (Integer, String), (Integer, String)) {
 template <typename X>
 void f(X && x) {
   emit<current::decay_t<X>>(std::forward<X>(x));
  }
};
#define ManuallyImplementedGenericPassThrough2(...) \
  RIPCURRENT_MACRO(ManuallyImplementedGenericPassThrough2, __VA_ARGS__)

// clang-format on

}  // namespace ripcurrent_unittest

TEST(RipCurrent, Passthrough) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | ManuallyImplementedGenericPassThrough1() | DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | ManuallyImplementedGenericPassThrough2() | DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | current::ripcurrent::Pass<Integer, String>() | DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | ManuallyImplementedGenericPassThrough1() | ManuallyImplementedGenericPassThrough2() |
     DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitIntegerAndString() | current::ripcurrent::Pass<Integer, String>() | ManuallyImplementedGenericPassThrough1() |
     ManuallyImplementedGenericPassThrough2() | RIPCURRENT_PASS(Integer, String) |
     current::ripcurrent::Pass<Integer, String>() | DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Join();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  EXPECT_EQ("... | ManuallyImplementedGenericPassThrough1() | ...",
            (ManuallyImplementedGenericPassThrough1()).Describe());
  EXPECT_EQ("... | { Integer, String } => ManuallyImplementedGenericPassThrough1() => { Integer, String } | ...",
            (ManuallyImplementedGenericPassThrough1()).DescribeWithTypes());
  EXPECT_EQ("... | ManuallyImplementedGenericPassThrough2() | ...",
            (ManuallyImplementedGenericPassThrough2()).Describe());
  EXPECT_EQ("... | { Integer, String } => ManuallyImplementedGenericPassThrough2() => { Integer, String } | ...",
            (ManuallyImplementedGenericPassThrough2()).DescribeWithTypes());

  EXPECT_EQ("... | PASS | ...", (current::ripcurrent::Pass<Integer, String>()).Describe());
  EXPECT_EQ("... | { Integer, String } => PASS => { Integer, String } | ...",
            (current::ripcurrent::Pass<Integer, String>()).DescribeWithTypes());

  EXPECT_EQ("... | Pass(Integer, String) | ...", (RIPCURRENT_PASS(Integer, String)).Describe());
  EXPECT_EQ("... | { Integer, String } => Pass(Integer, String) => { Integer, String } | ...",
            (RIPCURRENT_PASS(Integer, String)).DescribeWithTypes());
}

namespace ripcurrent_unittest {

CURRENT_STRUCT(RequestContainer) {
  CURRENT_FIELD(request, Request);  // Obviously, move-only, no copy allowed.
  // clang-format off
  CURRENT_CONSTRUCTOR(RequestContainer)(Request&& request) : request(std::move(request)) {}
  // clang-format on
};

RIPCURRENT_NODE(RCHTTPAcceptor, void, RequestContainer) {
  RCHTTPAcceptor(uint16_t port)
      : scope(HTTP(port)
                  .Register("/ripcurrent", [this](Request request) { emit<RequestContainer>(std::move(request)); })) {
    const std::string base_url = Printf("http://localhost:%d/ripcurrent", static_cast<int>(port));
    EXPECT_EQ("1\n", HTTP(GET(base_url)).body);
    EXPECT_EQ("2\n", HTTP(HEAD(base_url)).body);
    EXPECT_EQ("3\n", HTTP(POST(base_url, "OK")).body);
  }
  HTTPRoutesScope scope;
};
#define RCHTTPAcceptor(...) RIPCURRENT_MACRO(RCHTTPAcceptor, __VA_ARGS__)

RIPCURRENT_NODE(RCHTTPResponder, RequestContainer, void) {
  int index = 0;
  std::vector<std::string>& calls;
  // clang-format off
  // Messes with " & " -- D.K.
  RCHTTPResponder(std::vector<std::string>& calls) : calls(calls) {}
  // clang-format on
  void f(RequestContainer container) {
    if (container.request.method == "POST") {
      calls.push_back("POST " + container.request.body);
    } else {
      calls.push_back(container.request.method);
    }
    container.request(current::ToString(++index) + '\n');
  }
};
#define RCHTTPResponder(...) RIPCURRENT_MACRO(RCHTTPResponder, __VA_ARGS__)

}  // namespace ripcurrent_unittest

TEST(RipCurrent, CanHandleHTTPRequests) {
  current::time::ResetToZero();

  using namespace ripcurrent_unittest;
  const uint16_t port = FLAGS_ripcurrent_http_test_port;

  std::vector<std::string> calls;

  const auto job = RCHTTPAcceptor(port) | RCHTTPResponder(std::ref(calls));
  ASSERT_TRUE(calls.empty());

  // Begin running the job.
  const auto scope = std::move(job.RipCurrent().Async());

  // The first three calls are performed from the constructor of `RCHTTPAcceptor`, and thus synchronously.
  EXPECT_EQ("GET,HEAD,POST OK", current::strings::Join(calls, ','));

  // Now make another call and confirm it goes through all the way.
  EXPECT_EQ("4\n", HTTP(POST(Printf("http://localhost:%d/ripcurrent", static_cast<int>(port)), "ONE MORE")).body);
  EXPECT_EQ("GET,HEAD,POST OK,POST ONE MORE", current::strings::Join(calls, ','));
}

namespace ripcurrent_unittest {

// clang-format off
RIPCURRENT_NODE(RCEmitterWithTimestamps, void, Integer) {
  RCEmitterWithTimestamps(std::function<void(int, std::chrono::microseconds)>& post_callback,
                          std::function<void(int, std::chrono::microseconds)>& schedule_callback,
                          std::function<void(std::chrono::microseconds)>& head_callback) {
    post_callback = [this](int v, std::chrono::microseconds t) { post<Integer>(t, v); };
    schedule_callback = [this](int v, std::chrono::microseconds t) { schedule<Integer>(t, v); };
    head_callback = [this](std::chrono::microseconds t) { head(t); };
  }
};
// clang-format on
#define RCEmitterWithTimestamps(...) RIPCURRENT_MACRO(RCEmitterWithTimestamps, __VA_ARGS__)

}  // namespace ripcurrent_unittest

TEST(RipCurrent, TimestampsShouldBeStrictlyIncreasing) {
  using namespace ripcurrent_unittest;

  std::function<void(int, std::chrono::microseconds)> post;
  std::function<void(int, std::chrono::microseconds)> schedule;
  std::function<void(std::chrono::microseconds)> head;
  std::atomic_size_t counter(0u);
  std::vector<int> result;

  current::WaitableAtomic<std::string> captured_error_message;
  const auto mock_scope = current::Singleton<current::ripcurrent::RipCurrentMockableErrorHandler>().ScopedInjectHandler(
      [&captured_error_message](const std::string& error_message) { captured_error_message.SetValue(error_message); });

  EXPECT_EQ("", captured_error_message.GetValue());

  const auto scope = std::move((RCEmitterWithTimestamps(std::ref(post), std::ref(schedule), std::ref(head)) |
                                RCDump(std::ref(result), std::ref(counter)))
                                   .RipCurrent()
                                   .Async());

  EXPECT_EQ("", current::strings::Join(result, ','));
  post(1, std::chrono::microseconds(1));
  post(3, std::chrono::microseconds(3));

  EXPECT_EQ("", captured_error_message.GetValue());

  post(2, std::chrono::microseconds(2));

  captured_error_message.Wait([](const std::string& s) { return !s.empty(); });
  EXPECT_EQ("Expecting timestamp >= 4, seeing 2.",
            current::strings::Split(captured_error_message.GetValue(), '\t').back());

  while (counter != 2u) {
    std::this_thread::yield();
  }

  EXPECT_EQ("1,3", current::strings::Join(result, ','));
}

TEST(RipCurrent, SchedulingEventsIntoTheFuture) {
  using namespace ripcurrent_unittest;

  std::function<void(int, std::chrono::microseconds)> post;
  std::function<void(int, std::chrono::microseconds)> schedule;
  std::function<void(std::chrono::microseconds)> head;
  std::atomic_size_t counter(0u);
  std::vector<int> result;

  const auto scope = std::move((RCEmitterWithTimestamps(std::ref(post), std::ref(schedule), std::ref(head)) |
                                RCDump(std::ref(result), std::ref(counter)))
                                   .RipCurrent()
                                   .Async());

  post(4, std::chrono::microseconds(4));
  schedule(6, std::chrono::microseconds(6));
  post(5, std::chrono::microseconds(5));
  post(7, std::chrono::microseconds(7));

  while (counter != 4u) {
    std::this_thread::yield();
  }

  EXPECT_EQ("4,5,6,7", current::strings::Join(result, ','));
}

TEST(RipCurrent, SchedulingEventsIntoTheFutureAndUpdatingHead) {
  using namespace ripcurrent_unittest;

  std::function<void(int, std::chrono::microseconds)> post;
  std::function<void(int, std::chrono::microseconds)> schedule;
  std::function<void(std::chrono::microseconds)> head;
  std::atomic_size_t counter(0u);
  std::vector<int> result;

  const auto scope = std::move((RCEmitterWithTimestamps(std::ref(post), std::ref(schedule), std::ref(head)) |
                                RCDump(std::ref(result), std::ref(counter)))
                                   .RipCurrent()
                                   .Async());

  schedule(11, std::chrono::microseconds(11));
  schedule(19, std::chrono::microseconds(19));
  schedule(12, std::chrono::microseconds(12));
  schedule(17, std::chrono::microseconds(17));

  head(std::chrono::microseconds(11));

  while (counter != 1u) {
    std::this_thread::yield();
  }
  EXPECT_EQ("11", current::strings::Join(result, ','));

  head(std::chrono::microseconds(12));

  while (counter != 2u) {
    std::this_thread::yield();
  }
  EXPECT_EQ("11,12", current::strings::Join(result, ','));

  head(std::chrono::microseconds(18));

  while (counter != 3u) {
    std::this_thread::yield();
  }
  EXPECT_EQ("11,12,17", current::strings::Join(result, ','));

  head(std::chrono::microseconds(20));

  while (counter != 4u) {
    std::this_thread::yield();
  }
  EXPECT_EQ("11,12,17,19", current::strings::Join(result, ','));
}

namespace ripcurrent_unittest {

// clang-format off
RIPCURRENT_NODE_T(TemplatedEmitter, void, T) {
  TemplatedEmitter() {
    // Too bad we need this ugly syntax for now. -- D.K.
    RIPCURRENT_TEMPLATED_EMIT((T), T, LifeUniverseAndEverything());
  }
};
// clang-format on
#define TemplatedEmitter(...) RIPCURRENT_MACRO_T(TemplatedEmitter, __VA_ARGS__)

}  // namespace ripcurrent_unittest

TEST(RipCurrent, TemplatedStaticAsserts) {
  using namespace ripcurrent_unittest;

  using integer_emitter_t = RIPCURRENT_UNDERLYING_TYPE(TemplatedEmitter(Integer));

  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::LHSTypes<>, typename integer_emitter_t::input_t>),
                "");
  static_assert(
      sizeof(is_same_or_compile_error<current::ripcurrent::RHSTypes<Integer>, typename integer_emitter_t::output_t>),
      "");

  using string_emitter_t = RIPCURRENT_UNDERLYING_TYPE(TemplatedEmitter(String));

  static_assert(sizeof(is_same_or_compile_error<current::ripcurrent::LHSTypes<>, typename string_emitter_t::input_t>),
                "");
  static_assert(
      sizeof(is_same_or_compile_error<current::ripcurrent::RHSTypes<String>, typename string_emitter_t::output_t>), "");
}

TEST(RipCurrent, TemplatedDescriptions) {
  using namespace ripcurrent_unittest;

  const auto integer_emitter = TemplatedEmitter(Integer);
  const auto string_emitter = TemplatedEmitter(String);

  EXPECT_EQ("TemplatedEmitter<Integer>() | ...", integer_emitter.Describe());
  EXPECT_EQ("TemplatedEmitter<Integer>() => { Integer } | ...", integer_emitter.DescribeWithTypes());
  EXPECT_EQ("TemplatedEmitter<String>() | ...", string_emitter.Describe());
  EXPECT_EQ("TemplatedEmitter<String>() => { String } | ...", string_emitter.DescribeWithTypes());
}

TEST(RipCurrent, TemplatedFlow) {
  using namespace ripcurrent_unittest;

  std::vector<std::string> result;
  ((TemplatedEmitter(Integer) + TemplatedEmitter(String)) | DumpIntegerAndString(std::ref(result))).RipCurrent().Join();
  EXPECT_EQ("42, 'The Answer'", current::strings::Join(result, ", "));
}
