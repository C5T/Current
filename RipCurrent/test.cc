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

#include "ripcurrent.h"

#include "../Bricks/dflags/dflags.h"

#include "../Bricks/strings/join.h"
#include "../Bricks/strings/split.h"

#include "../Blocks/HTTP/api.h"

#include "../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_uint16(ripcurrent_http_test_port, PickPortForUnitTest(), "Local port to use for RipCurrent unit test.");

// `clang-format` messes up macro-defined class definitions, so disable it temporarily for this section. -- D.K.

// clang-format off
namespace ripcurrent_unittest {

CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(value, int32_t);
  CURRENT_CONSTRUCTOR(Integer)(int32_t value = 0) : value(value) {}
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
  RCDump() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  RCDump(std::vector<int>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(x.value);
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
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  (RCEmit(1, 2, 3) | RCDump(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("1,2,3", current::strings::Join(result, ','));
}

TEST(RipCurrent, SingleChainFlow) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  (RCEmit(1, 2, 3) | RCMult(2) | RCMult(5) | RCMult(10) | RCDump(std::ref(result))).RipCurrent().Sync();
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
  emit_dump.RipCurrent().Sync();
  EXPECT_EQ("42", current::strings::Join(result, ','));
}

TEST(RipCurrent, OrderDoesNotMatter) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  const auto a = RCEmit(1);
  const auto b = RCMult(2);
  const auto c = RCDump(std::ref(result));

  result.clear();
  (a | b | b | c).RipCurrent().Sync();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  ((a | b) | (b | c)).RipCurrent().Sync();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  ((a | (b | b)) | c).RipCurrent().Sync();
  EXPECT_EQ("4", current::strings::Join(result, ','));

  result.clear();
  (a | ((b | b) | c)).RipCurrent().Sync();
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

  (emit1 | mult10 | dump1).RipCurrent().Sync();
  (emit2 | mult10 | dump2).RipCurrent().Sync();
  EXPECT_EQ("10", current::strings::Join(result1, ','));
  EXPECT_EQ("20", current::strings::Join(result2, ','));

  result1.clear();
  result2.clear();
  ((emit1 | mult10) | dump1).RipCurrent().Sync();
  ((emit2 | mult10) | dump2).RipCurrent().Sync();
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
  static_assert(std::is_same<RCEmit::input_t, current::ripcurrent::LHSTypes<>>::value, "");
  static_assert(std::is_same<RCEmit::output_t, current::ripcurrent::RHSTypes<Integer>>::value, "");

  EXPECT_EQ("RCEmit", RIPCURRENT_UNDERLYING_TYPE(RCEmit())::UnitTestClassName());

  const auto emit = RCEmit();
  EXPECT_EQ("RCEmit", RIPCURRENT_UNDERLYING_TYPE(emit)::UnitTestClassName());

  EXPECT_EQ("RCMult", RCMult::UnitTestClassName());
  static_assert(std::is_same<RCMult::input_t, current::ripcurrent::LHSTypes<Integer>>::value, "");
  static_assert(std::is_same<RCMult::output_t, current::ripcurrent::RHSTypes<Integer>>::value, "");

  EXPECT_EQ("RCMult", RIPCURRENT_UNDERLYING_TYPE(RCMult())::UnitTestClassName());

  const auto mult = RCMult();
  EXPECT_EQ("RCMult", RIPCURRENT_UNDERLYING_TYPE(mult)::UnitTestClassName());

  EXPECT_EQ("RCDump", RCDump::UnitTestClassName());
  static_assert(std::is_same<RCDump::input_t, current::ripcurrent::LHSTypes<Integer>>::value, "");
  static_assert(std::is_same<RCDump::output_t, current::ripcurrent::RHSTypes<>>::value, "");

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

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult)::input_t, current::ripcurrent::LHSTypes<>>::value,
                "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult)::output_t, current::ripcurrent::RHSTypes<Integer>>::value, "");

  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(mult_dump)::input_t, current::ripcurrent::LHSTypes<Integer>>::value, "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(mult_dump)::output_t, current::ripcurrent::RHSTypes<>>::value,
                "");

  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_dump)::input_t, current::ripcurrent::LHSTypes<>>::value,
                "");
  static_assert(std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_dump)::output_t, current::ripcurrent::RHSTypes<>>::value,
                "");

  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_1)::input_t, current::ripcurrent::LHSTypes<>>::value, "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_1)::output_t, current::ripcurrent::RHSTypes<>>::value, "");

  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_2)::input_t, current::ripcurrent::LHSTypes<>>::value, "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_dump_2)::output_t, current::ripcurrent::RHSTypes<>>::value, "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_1)::input_t, current::ripcurrent::LHSTypes<>>::value,
      "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_1)::output_t, current::ripcurrent::RHSTypes<>>::value,
      "");

  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_2)::input_t, current::ripcurrent::LHSTypes<>>::value,
      "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_2)::output_t, current::ripcurrent::RHSTypes<>>::value,
      "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_3)::input_t, current::ripcurrent::LHSTypes<>>::value,
      "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_3)::output_t, current::ripcurrent::RHSTypes<>>::value,
      "");

  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_4)::input_t, current::ripcurrent::LHSTypes<>>::value,
      "");
  static_assert(
      std::is_same<RIPCURRENT_UNDERLYING_TYPE(emit_mult_mult_dump_4)::output_t, current::ripcurrent::RHSTypes<>>::value,
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
};

RIPCURRENT_NODE(EmitStringAndInteger, (), (Integer, String)) {  // Note: `()` is same as `void`.
  EmitStringAndInteger() {
    emit<String>("Answer");
    emit<Integer>(42);
  }
};
#define EmitStringAndInteger(...) RIPCURRENT_MACRO(EmitStringAndInteger, __VA_ARGS__)

RIPCURRENT_NODE(MultIntegerOrString, (Integer, String), (Integer, String)) {
  const int k;
  MultIntegerOrString(int k = 101) : k(k) {}
  void f(Integer x) {
    emit<Integer>(x.value * k);
  }
  void f(String x) {
    emit<String>("Yo? " + x.value + " Yo!");
  }
};
#define MultIntegerOrString(...) RIPCURRENT_MACRO(MultIntegerOrString, __VA_ARGS__)

RIPCURRENT_NODE(DumpIntegerAndString, (Integer, String), ()) {  // Note: `()` is same as `void`.
  std::vector<std::string>* ptr;
  DumpIntegerAndString() : ptr(nullptr) {}  // LCOV_EXCL_LINE
  DumpIntegerAndString(std::vector<std::string>& ref) : ptr(&ref) {}
  void f(Integer x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back(current::ToString(x.value));
  }
  void f(String x) {
    CURRENT_ASSERT(ptr);
    ptr->push_back("'" + x.value + "'");
  }
};
#define DumpIntegerAndString(...) RIPCURRENT_MACRO(DumpIntegerAndString, __VA_ARGS__)

}  // namespace ripcurrent_unittest
// clang-format on

TEST(RipCurrent, CustomTypesIntrospection) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("EmitStringAndInteger() => { Integer, String } | ...", (EmitStringAndInteger()).DescribeWithTypes());
  EXPECT_EQ("... | { Integer, String } => DumpIntegerAndString()", (DumpIntegerAndString()).DescribeWithTypes());
}

TEST(RipCurrent, CustomTypesFlow) {
  using namespace ripcurrent_unittest;

  {
    std::vector<std::string> result;
    (EmitStringAndInteger() | DumpIntegerAndString(std::ref(result))).RipCurrent().Sync();
    EXPECT_EQ("'Answer', 42", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitStringAndInteger() | MultIntegerOrString() | DumpIntegerAndString(std::ref(result))).RipCurrent().Sync();
    EXPECT_EQ("'Yo? Answer Yo!', 4242", current::strings::Join(result, ", "));
  }

  {
    std::vector<std::string> result;
    (EmitStringAndInteger() | MultIntegerOrString() | MultIntegerOrString(10001) |
     DumpIntegerAndString(std::ref(result)))
        .RipCurrent()
        .Sync();
    EXPECT_EQ("'Yo? Yo? Answer Yo! Yo!', 42424242", current::strings::Join(result, ", "));
  }
}

namespace ripcurrent_unittest {

RIPCURRENT_NODE(RCHTTPAcceptor, void, Request) {
  RCHTTPAcceptor(uint16_t port)
      : scope(HTTP(port).Register("/ripcurrent", [this](Request r) { emit<Request>(std::move(r)); })) {
    const std::string base_url = Printf("http://localhost:%d/ripcurrent", static_cast<int>(port));
    EXPECT_EQ("OK\n", HTTP(GET(base_url)).body);
    EXPECT_EQ("OK\n", HTTP(HEAD(base_url)).body);
    EXPECT_EQ("OK\n", HTTP(POST(base_url, "OK")).body);
  }
  HTTPRoutesScope scope;
};
#define RCHTTPAcceptor(...) RIPCURRENT_MACRO(RCHTTPAcceptor, __VA_ARGS__)

RIPCURRENT_NODE(RCHTTPResponder, Request, void) {
  std::vector<std::string>& requests;
  // clang-format off
  // Messes with " & " -- D.K.
  RCHTTPResponder(std::vector<std::string>& requests) : requests(requests) {}
  // clang-format on
  void f(Request r) {
    if (r.method == "POST") {
      requests.push_back("POST " + r.body);
    } else {
      requests.push_back(r.method);
    }
    r("OK\n");
  }
};
#define RCHTTPResponder(...) RIPCURRENT_MACRO(RCHTTPResponder, __VA_ARGS__)

}  // namespace ripcurrent_unittest

TEST(RipCurrent, CanHandleHTTPRequest) {
  using namespace ripcurrent_unittest;

  std::vector<std::string> results;
  (RCHTTPAcceptor(FLAGS_ripcurrent_http_test_port) | RCHTTPResponder(std::ref(results))).RipCurrent().Sync();
  EXPECT_EQ("GET,HEAD,POST OK", current::strings::Join(results, ','));
}
