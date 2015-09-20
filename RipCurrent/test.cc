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

#include "../Bricks/strings/join.h"

#include "../3rdparty/gtest/gtest-main.h"

using bricks::strings::Join;

// `clang-format` messes up macro-defined class definitions, so disable it temporarily for this section. -- D.K.

// clang-format off

namespace ripcurrent_unittest {

typedef int Integer;

/*
// TODO(dkorolev): Typing and RTTI for messages flowing through is coming soon.
struct Integer : H2O {
  int value = 0;
  Integer() = default;
  Integer(int value) : value(value) {}
};
struct String : H2O {
  ...
};
*/

// `Foo`: The emitter of events. Emits the integers passed to its constructor.
CURRENT_LHS(Foo, Integer) {
  static std::string UnitTestClassName() { return "Foo"; }
  Foo() {}
  Foo(int a) { emit(Integer(a)); }
  Foo(int a, int b) {
    emit(Integer(a));
    emit(Integer(b));
  }
  Foo(int a, int b, int c) {
    emit(Integer(a));
    emit(Integer(b));
    emit(Integer(c));
  }
};
#define Foo(...) REGISTER_LHS(Foo, __VA_ARGS__)

// `Bar`: The processor of events. Multiplies each integer by what was passed to its constructor.
CURRENT_VIA(Bar, int) {
  static std::string UnitTestClassName() { return "Bar"; }
  int k;
  Bar(int k = 1) : k(k) {}
  void f(Integer x) { emit(Integer(x * k)); }
};
#define Bar(...) REGISTER_VIA(Bar, __VA_ARGS__)

// `Baz`: The destination of events. Collects the output integers.
CURRENT_RHS(Baz) {
  static std::string UnitTestClassName() { return "Baz"; }
  std::vector<int>* ptr;
  Baz() : ptr(nullptr) {}
  Baz(std::vector<int>& ref) : ptr(&ref) {}
  void f(Integer x) {
    assert(ptr);
    ptr->push_back(x);
  }
};
#define Baz(...) REGISTER_RHS(Baz, __VA_ARGS__)

}  // namespace ripcurrent_unittest

// clang-format on

TEST(RipCurrent, NotLeftHanging) {
  // This test is best to keep first, since it depends on line numbers. -- D.K.

  using namespace ripcurrent_unittest;

  std::string captured_error_message;
  const auto scope = bricks::Singleton<RipCurrentMockableErrorHandler>().ScopedInjectHandler(
      [&captured_error_message](const std::string& error_message) { captured_error_message = error_message; });

  EXPECT_EQ("", captured_error_message);
  Foo();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "Foo() | ...\n"
      "Foo() from test.cc:106\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);
  Bar();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | Bar() | ...\n"
      "Bar() from test.cc:113\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);
  Baz();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | Baz()\n"
      "Baz() from test.cc:120\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);
  Foo(1) | Bar(2);
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "Foo(1) | Bar(2) | ...\n"
      "Foo(1) from test.cc:127\n"
      "Bar(2) from test.cc:127\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);
  Bar(3) | Baz();
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "... | Bar(3) | Baz()\n"
      "Bar(3) from test.cc:135\n"
      "Baz() from test.cc:135\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);
  std::vector<int> result;
  Foo(42) | Bar(100) | Baz(std::ref(result));
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "Foo(42) | Bar(100) | Baz(std::ref(result))\n"
      "Foo(42) from test.cc:144\n"
      "Bar(100) from test.cc:144\n"
      "Baz(std::ref(result)) from test.cc:144\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);

  { const auto tmp = Foo() | Bar(1) | Baz(std::ref(result)); }
  EXPECT_EQ(
      "RipCurrent building block leaked.\n"
      "Foo() | Bar(1) | Baz(std::ref(result))\n"
      "Foo() from test.cc:154\n"
      "Bar(1) from test.cc:154\n"
      "Baz(std::ref(result)) from test.cc:154\n"
      "Each building block should be run, described, used as part of a larger block, or dismissed.\n",
      captured_error_message);

  captured_error_message = "NO ERROR";
  {
    const auto tmp = Foo() | Bar(2) | Baz(std::ref(result));
    tmp.Describe();
  }
  EXPECT_EQ("NO ERROR", captured_error_message);

  captured_error_message = "NO ERROR ONCE AGAIN";
  {
    const auto tmp = Foo() | Bar(3) | Baz(std::ref(result));
    tmp.Dismiss();
  }
  EXPECT_EQ("NO ERROR ONCE AGAIN", captured_error_message);
}

TEST(RipCurrent, SingleEdgeFlow) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;
  (Foo(1, 2, 3) | Baz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("1,2,3", Join(result, ','));
}

TEST(RipCurrent, SingleChainFlow) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;
  (Foo(1, 2, 3) | Bar(10) | Bar(10) | Baz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("100,200,300", Join(result, ','));
}

TEST(RipCurrent, DeclarationDoesNotRunConstructors) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  RipCurrentLHS<Integer> foo = Foo(42);
  EXPECT_EQ("Foo(42) | ...", foo.Describe());

  RipCurrentRHS baz = Baz(std::ref(result));
  EXPECT_EQ("... | Baz(std::ref(result))", baz.Describe());

  RipCurrent foo_baz = foo | baz;
  EXPECT_EQ("Foo(42) | Baz(std::ref(result))", foo_baz.Describe());

  EXPECT_EQ("", Join(result, ','));
  foo_baz.RipCurrent().Sync();
  EXPECT_EQ("42", Join(result, ','));
}

TEST(RipCurrent, OrderDoesNotMatter) {
  using namespace ripcurrent_unittest;

  std::vector<int> result;

  result.clear();
  (Foo(1) | Bar(10) | Baz(std::ref(result))).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result, ','));

  const auto a = Foo(1);
  const auto b = Bar(2);
  const auto c = Baz(std::ref(result));

  result.clear();
  (a | b | b | c).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  ((a | b) | (b | c)).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  ((a | (b | b)) | c).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));

  result.clear();
  (a | ((b | b) | c)).RipCurrent().Sync();
  EXPECT_EQ("4", Join(result, ','));
}

TEST(RipCurrent, BuildingBlocksCanBeReUsed) {
  using namespace ripcurrent_unittest;

  std::vector<int> result1;
  std::vector<int> result2;

  RipCurrentLHS<Integer> foo1 = Foo(1);
  RipCurrentLHS<Integer> foo2 = Foo(2);
  RipCurrentVIA<Integer> bar = Bar(10);
  RipCurrentRHS baz1 = Baz(std::ref(result1));
  RipCurrentRHS baz2 = Baz(std::ref(result2));

  (foo1 | bar | baz1).RipCurrent().Sync();
  (foo2 | bar | baz2).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result1, ','));
  EXPECT_EQ("20", Join(result2, ','));

  result1.clear();
  result2.clear();
  ((foo1 | bar) | baz1).RipCurrent().Sync();
  ((foo2 | bar) | baz2).RipCurrent().Sync();
  EXPECT_EQ("10", Join(result1, ','));
  EXPECT_EQ("20", Join(result2, ','));
}

TEST(RipCurrent, SynopsisAndDecorators) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("Foo() | ...", (Foo()).Describe());
  EXPECT_EQ("... | Bar() | ...", (Bar()).Describe());
  EXPECT_EQ("... | Baz()", (Baz()).Describe());

  EXPECT_EQ("Foo() | Baz()", (Foo() | Baz()).Describe());
  EXPECT_EQ("Foo() | Bar() | Bar() | Bar() | Baz()", (Foo() | Bar() | Bar() | Bar() | Baz()).Describe());

  EXPECT_EQ("Foo() | Bar() | ...", (Foo() | Bar()).Describe());
  EXPECT_EQ("Foo() | Bar() | Bar() | Bar() | ...", (Foo() | Bar() | Bar() | Bar()).Describe());

  EXPECT_EQ("... | Bar() | Baz()", (Bar() | Baz()).Describe());
  EXPECT_EQ("... | Bar() | Bar() | Bar() | Baz()", (Bar() | Bar() | Bar() | Baz()).Describe());

  EXPECT_EQ("... | Bar() | Bar() | Bar() | ...", (Bar() | Bar() | Bar()).Describe());

  const int blah = 5;
  EXPECT_EQ("Foo(1) | Bar(2) | Bar(3 + 4) | Bar(blah) | Baz()",
            (Foo(1) | Bar(2) | Bar(3 + 4) | Bar(blah) | Baz()).Describe());

  const int x = 1;
  const int y = 1;
  const int z = 1;
  EXPECT_EQ("Foo(x, y, z) | Baz()", (Foo(x, y, z) | Baz()).Describe());
}

TEST(RipCurrent, TypeSystemGuarantees) {
  using namespace ripcurrent_unittest;

  EXPECT_EQ("Foo", Foo::UnitTestClassName());
  static_assert(Foo::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<Foo::OUTPUT_TYPES_AS_TYPELIST, TypeList<Integer>>::value, "");

  EXPECT_EQ("Foo", CURRENT_USER_TYPE(Foo())::UnitTestClassName());

  const auto foo = Foo();
  EXPECT_EQ("Foo", CURRENT_USER_TYPE(foo)::UnitTestClassName());

  EXPECT_EQ("Bar", Bar::UnitTestClassName());
  static_assert(Bar::INPUT_POLICY == InputPolicy::Accepts, "");
  static_assert(std::is_same<Bar::OUTPUT_TYPES_AS_TYPELIST, TypeList<Integer>>::value, "");

  EXPECT_EQ("Bar", CURRENT_USER_TYPE(Bar())::UnitTestClassName());

  const auto bar = Bar();
  EXPECT_EQ("Bar", CURRENT_USER_TYPE(bar)::UnitTestClassName());

  EXPECT_EQ("Baz", Baz::UnitTestClassName());
  static_assert(Baz::INPUT_POLICY == InputPolicy::Accepts, "");
  static_assert(std::is_same<Baz::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value, "");

  EXPECT_EQ("Baz", CURRENT_USER_TYPE(Baz())::UnitTestClassName());

  const auto baz = Baz();
  EXPECT_EQ("Baz", CURRENT_USER_TYPE(baz)::UnitTestClassName());

  const auto foo_bar = foo | bar;
  const auto bar_baz = bar | baz;
  const auto foo_baz = foo | baz;
  const auto foo_bar_baz_1 = (foo | bar) | baz;
  const auto foo_bar_baz_2 = foo | (bar | baz);
  const auto foo_bar_bar_baz_1 = (foo | bar) | (bar | baz);
  const auto foo_bar_bar_baz_2 = foo | (bar | bar) | baz;
  const auto foo_bar_bar_baz_3 = ((foo | bar) | bar) | baz;
  const auto foo_bar_bar_baz_4 = foo | (bar | (bar | baz));

  static_assert(CURRENT_USER_TYPE(foo_bar)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar)::OUTPUT_TYPES_AS_TYPELIST, TypeList<Integer>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(bar_baz)::INPUT_POLICY == InputPolicy::Accepts, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(bar_baz)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value, "");

  static_assert(CURRENT_USER_TYPE(foo_baz)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_baz)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value, "");

  static_assert(CURRENT_USER_TYPE(foo_bar_baz_1)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_baz_1)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(foo_bar_baz_2)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_baz_2)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(foo_bar_bar_baz_1)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_bar_baz_1)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(foo_bar_bar_baz_2)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_bar_baz_2)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(foo_bar_bar_baz_3)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_bar_baz_3)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  static_assert(CURRENT_USER_TYPE(foo_bar_bar_baz_4)::INPUT_POLICY == InputPolicy::DoesNotAccept, "");
  static_assert(std::is_same<CURRENT_USER_TYPE(foo_bar_bar_baz_4)::OUTPUT_TYPES_AS_TYPELIST, TypeList<>>::value,
                "");

  foo_bar.Dismiss();
  bar_baz.Dismiss();
  foo_baz.Dismiss();
  foo_bar_baz_1.Dismiss();
  foo_bar_baz_2.Dismiss();
  foo_bar_bar_baz_1.Dismiss();
  foo_bar_bar_baz_2.Dismiss();
  foo_bar_bar_baz_3.Dismiss();
  foo_bar_bar_baz_4.Dismiss();
}
