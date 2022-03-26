/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2019 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "call_all_constructors.h"
#include "call_if.h"
#include "is_unique_ptr.h"
#include "variadic_indexes.h"

// The internal test uses `std::tuple<>`, and not a single `TypeList`.
// The one that goes into the documentation uses `TypeList<>`, and not a single `std::tuple<>`.

#include "docu/docu_05metaprogramming_02.cc"

// The code below uses some definitions from the `docu/*.cc` file `#include`-d above,
// namely `add_100`, `y_is_even`, `concatenate_s`, and `UserFriendlyArithmetics` et al.

TEST(TemplateMetaprogrammingInternalTest, Map) {
  struct A {
    enum { x = 1 };
  };
  struct B {
    enum { x = 2 };
  };
  struct C {
    enum { x = 3 };
  };

  std::tuple<A, B, C> before;
  static_assert(std::tuple_size<decltype(before)>::value == 3, "");
  EXPECT_EQ(1, std::get<0>(before).x);
  EXPECT_EQ(2, std::get<1>(before).x);
  EXPECT_EQ(3, std::get<2>(before).x);

  current::metaprogramming::map<add_100, decltype(before)> after;
  static_assert(std::tuple_size<decltype(after)>::value == 3, "");
  EXPECT_EQ(101, std::get<0>(after).x);
  EXPECT_EQ(102, std::get<1>(after).x);
  EXPECT_EQ(103, std::get<2>(after).x);
}

TEST(TemplateMetaprogrammingInternalTest, Filter) {
  struct A {
    enum { y = 10 };
  };
  struct B {
    enum { y = 15 };
  };
  struct C {
    enum { y = 20 };
  };

  std::tuple<A, B, C> before;
  static_assert(std::tuple_size<decltype(before)>::value == 3, "");
  EXPECT_EQ(10, std::get<0>(before).y);
  EXPECT_EQ(15, std::get<1>(before).y);
  EXPECT_EQ(20, std::get<2>(before).y);

  current::metaprogramming::filter<y_is_even, decltype(before)> after;
  static_assert(std::tuple_size<decltype(after)>::value == 2, "");
  EXPECT_EQ(10, std::get<0>(after).y);
  EXPECT_EQ(20, std::get<1>(after).y);
}

TEST(TemplateMetaprogrammingInternalTest, Reduce) {
  struct A {
    static std::string s() { return "A"; }
  };
  struct B {
    static std::string s() { return "B"; }
  };
  struct C {
    static std::string s() { return "C"; }
  };
  EXPECT_EQ("(A+(B+C))", (current::metaprogramming::reduce<concatenate_s, std::tuple<A, B, C>>::s()));
}

TEST(TemplateMetaprogrammingInternalTest, Combine) {
  EXPECT_EQ(1, NEG()(NEG::TYPE(), -1));
  EXPECT_EQ(3, ADD()(ADD::TYPE(), 1, 2));
  EXPECT_EQ(6, ADD()(ADD::TYPE(), 1, 2, "3"));
  EXPECT_EQ(20, MUL()(MUL::TYPE(), 4, 5));
  EXPECT_EQ(120, MUL()(MUL::TYPE(), 4, 5, 6));

  // As a sanity check, since `MUL` inherits from `ADD`,
  // the following construct will work just fine.
  EXPECT_EQ(15, MUL().ADD::operator()(ADD::TYPE(), 7, 8));

  typedef current::metaprogramming::combine<std::tuple<NEG, ADD, MUL>> Arithmetics;
  EXPECT_EQ(-1, Arithmetics()(NEG::TYPE(), 1));
  EXPECT_EQ(5, Arithmetics()(ADD::TYPE(), 2, 3));
  EXPECT_EQ(9, Arithmetics()(ADD::TYPE(), 2, 3, "4"));
  EXPECT_EQ(30, Arithmetics()(MUL::TYPE(), 5, 6));
  EXPECT_EQ(210, Arithmetics()(MUL::TYPE(), 5, 6, 7));

  // Using the dispatched methods.
  EXPECT_EQ(42, UserFriendlyArithmetics().Neg(-42));
  EXPECT_EQ(21, UserFriendlyArithmetics().Add(10, 11));
  EXPECT_EQ(33, UserFriendlyArithmetics().Add(10, 11, "12"));
  EXPECT_EQ(420, UserFriendlyArithmetics().Mul(20, 21));
  EXPECT_EQ(9240, UserFriendlyArithmetics().Mul(20, 21, 22));

  // The following call will fail to compile,
  // with a nice error message explaining
  // that none of the `NEG`, `ADD` and `MUL`
  // have division operation defined.
  //
  // UserFriendlyArithmetics().Div(100, 5);

  struct Magic {
    std::string result;
  };
  Magic magic;
  UserFriendlyArithmetics().DispatchToAll(magic);
  EXPECT_EQ("NEG\nADD\nMUL\n", magic.result);
}

TEST(TemplateMetaprogrammingInternalTest, VariadicIndexes) {
  using current::variadic_indexes::generate_indexes;
  using current::variadic_indexes::indexes;

  static_assert(std::is_same_v<indexes<>, generate_indexes<0>>, "");
  static_assert(std::is_same_v<indexes<0>, generate_indexes<1>>, "");
  static_assert(std::is_same_v<indexes<0, 1>, generate_indexes<2>>, "");
  static_assert(std::is_same_v<indexes<0, 1, 2>, generate_indexes<3>>, "");
}

TEST(TemplateMetaprogrammingInternalTest, OddIndexes) {
  using current::metaprogramming::EvensOnly;
  struct A {};
  struct B {};
  struct C {};
  struct D {};
  struct E {};
  static_assert(std::is_same_v<TypeList<>, EvensOnly<TypeList<>>>, "");
  static_assert(std::is_same_v<TypeList<A>, EvensOnly<TypeList<A>>>, "");
  static_assert(std::is_same_v<TypeList<A>, EvensOnly<TypeList<A, B>>>, "");
  static_assert(std::is_same_v<TypeList<A, C>, EvensOnly<TypeList<A, B, C>>>, "");
  static_assert(std::is_same_v<TypeList<A, C>, EvensOnly<TypeList<A, B, C, D>>>, "");
  static_assert(std::is_same_v<TypeList<A, C, E>, EvensOnly<TypeList<A, B, C, D, E>>>, "");
};

TEST(TemplateMetaprogrammingInternalTest, CallIf) {
  using current::metaprogramming::CallIf;

  int i = 0;
  const auto lambda = [&i]() { ++i; };

  CallIf<false>::With(lambda);
  EXPECT_EQ(0, i);
  CallIf<true>::With(lambda);
  EXPECT_EQ(1, i);
  CallIf<false>::With(lambda);
  EXPECT_EQ(1, i);
  CallIf<true>::With(lambda);
  EXPECT_EQ(2, i);
}

namespace metaprogramming_unittest {

struct AddOneToInt {
  constexpr static int value_to_add = 1;
};
struct AddTwoToInt {
  constexpr static int value_to_add = 2;
};

struct SingletonAdderValue {
  int value = 0;
};

template <typename T>
struct AdderToSingleton {
  AdderToSingleton() { current::Singleton<SingletonAdderValue>().value += T::value_to_add; }
};

template <typename T>
struct AdderToInt {
  AdderToInt(int& x) { x += T::value_to_add; }
};

}  // namespace metaprogramming_unittest

TEST(TemplateMetaprogrammingInternalTest, CallAllConstructors) {
  using namespace metaprogramming_unittest;
  using current::metaprogramming::call_all_constructors;
  {
    current::Singleton<SingletonAdderValue>().value = 0;
    call_all_constructors<AdderToSingleton, TypeListImpl<AddOneToInt>>();
    EXPECT_EQ(1, current::Singleton<SingletonAdderValue>().value);
  }
  {
    current::Singleton<SingletonAdderValue>().value = 0;
    call_all_constructors<AdderToSingleton, TypeListImpl<AddTwoToInt>>();
    EXPECT_EQ(2, current::Singleton<SingletonAdderValue>().value);
  }
  {
    current::Singleton<SingletonAdderValue>().value = 0;
    call_all_constructors<AdderToSingleton, TypeListImpl<AddOneToInt, AddTwoToInt>>();
    EXPECT_EQ(3, current::Singleton<SingletonAdderValue>().value);
  }
  {
    current::Singleton<SingletonAdderValue>().value = 0;
    call_all_constructors<AdderToSingleton, TypeListImpl<AddTwoToInt, AddOneToInt>>();
    EXPECT_EQ(3, current::Singleton<SingletonAdderValue>().value);
  }
}

TEST(TemplateMetaprogrammingInternalTest, CallAllConstructorsWith) {
  using namespace metaprogramming_unittest;
  using current::metaprogramming::call_all_constructors_with;
  {
    int x = 0;
    call_all_constructors_with<AdderToInt, int, TypeListImpl<AddOneToInt>>(x);
    EXPECT_EQ(1, x);
  }
  {
    int x = 0;
    call_all_constructors_with<AdderToInt, int, TypeListImpl<AddTwoToInt>>(x);
    EXPECT_EQ(2, x);
  }
  {
    int x = 0;
    call_all_constructors_with<AdderToInt, int, TypeListImpl<AddOneToInt, AddTwoToInt>>(x);
    EXPECT_EQ(3, x);
  }
  {
    int x = 0;
    call_all_constructors_with<AdderToInt, int, TypeListImpl<AddTwoToInt, AddOneToInt>>(x);
    EXPECT_EQ(3, x);
  }
}

namespace metaprogramming_unittest {

struct ExtractString {
  const std::string s;
  ExtractString() = delete;
  ExtractString(const std::pair<std::string, int>& pair) : s(pair.first) {}
  const std::string& operator()(const std::string&) const { return s; }
};

struct ExtractInt {
  const int i;
  ExtractInt() = delete;
  ExtractInt(const std::pair<std::string, int>& pair) : i(pair.second) {}
  int operator()(int) const { return i; }
};

}  // namespace metaprogramming_unittest

TEST(TemplateMetaprogrammingInternalTest, NonemptyConstructorForCombiner) {
  using namespace metaprogramming_unittest;
  const current::metaprogramming::combine<TypeList<ExtractString, ExtractInt>> joined(std::make_pair("foo", 42));
  EXPECT_EQ("foo", joined("test"));
  EXPECT_EQ(42, joined(0));
}

// ********************************************************************************
// * `current::decay_t` test, moved here from `decay.h`.
// ********************************************************************************
namespace current_decay_t_test {

using current::decay_t;
using std::is_same_v;

static_assert(is_same_v<int, decay_t<int>>, "");
static_assert(is_same_v<int, decay_t<int&>>, "");
static_assert(is_same_v<int, decay_t<int&&>>, "");
static_assert(is_same_v<int, decay_t<const int>>, "");
static_assert(is_same_v<int, decay_t<const int&>>, "");
static_assert(is_same_v<int, decay_t<const int&&>>, "");

static_assert(is_same_v<std::string, decay_t<std::string>>, "");
static_assert(is_same_v<std::string, decay_t<std::string&>>, "");
static_assert(is_same_v<std::string, decay_t<std::string&&>>, "");
static_assert(is_same_v<std::string, decay_t<const std::string>>, "");
static_assert(is_same_v<std::string, decay_t<const std::string&>>, "");
static_assert(is_same_v<std::string, decay_t<const std::string&&>>, "");

static_assert(is_same_v<char*, decay_t<char*>>, "");
static_assert(is_same_v<char*, decay_t<char*&>>, "");
static_assert(is_same_v<char*, decay_t<char*&&>>, "");
static_assert(is_same_v<const char*, decay_t<const char*>>, "");
static_assert(is_same_v<const char*, decay_t<char const*>>, "");
static_assert(is_same_v<char*, decay_t<char* const>>, "");
static_assert(is_same_v<const char*, decay_t<const char*&>>, "");
static_assert(is_same_v<const char*, decay_t<char const*&>>, "");
static_assert(is_same_v<char*, decay_t<char* const&>>, "");
static_assert(is_same_v<const char*, decay_t<const char*&&>>, "");
static_assert(is_same_v<const char*, decay_t<char const*&&>>, "");
static_assert(is_same_v<char*, decay_t<char* const&&>>, "");
// NOTE(mzhurovich): Previous implementation was decaying literal to `char[n]`.
static_assert(is_same_v<const char*, decay_t<decltype("test")>>, "");

// `current::decay_t` + `std::tuple`.
static_assert(is_same_v<std::tuple<int, int, int, int>, decay_t<std::tuple<int, int, int, int>>>, "");
static_assert(is_same_v<std::tuple<int, int, int, int>, decay_t<std::tuple<const int, int&, const int&, int&&>>>, "");

static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<const std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<std::string&&>>>, "");

static_assert(is_same_v<std::tuple<std::string>, decay_t<const std::tuple<std::string>&>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<const std::tuple<const std::string>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<const std::tuple<const std::string&>>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<const std::tuple<const std::string&>&>>, "");
static_assert(is_same_v<std::tuple<std::string>, decay_t<std::tuple<const std::string&>&&>>, "");

static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<std::tuple<std::tuple<const std::string>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<std::tuple<std::tuple<std::string&>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<std::tuple<std::tuple<const std::string&>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<std::tuple<std::tuple<std::string&&>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<std::tuple<std::tuple<std::string>>>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<const std::tuple<std::tuple<std::string>>&>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<const std::tuple<std::tuple<std::string>&>&>>, "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<const std::tuple<std::tuple<std::string&>&>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<std::string>>, decay_t<const std::tuple<std::tuple<std::string&&>&>&>>,
              "");
static_assert(is_same_v<std::tuple<std::tuple<int>, std::tuple<int>, std::tuple<int>, std::tuple<int>>,
                        decay_t<std::tuple<const std::tuple<const int>,
                                           std::tuple<int&>&,
                                           const std::tuple<const int&>&,
                                           std::tuple<int&&>&&>>>,
              "");

}  // namespace current_decay_t_test

// ********************************************************************************
// * `current::is_unique_ptr` test, moved here from `is_unique_ptr.h`.
// ********************************************************************************
namespace current_is_unique_ptr_test {

using current::is_unique_ptr;

static_assert(!is_unique_ptr<int>::value, "");
static_assert(std::is_same_v<int, typename is_unique_ptr<int>::underlying_type>, "");

static_assert(is_unique_ptr<std::unique_ptr<int>>::value, "");
static_assert(std::is_same_v<int, typename is_unique_ptr<std::unique_ptr<int>>::underlying_type>, "");

}  // namespace current_is_unique_ptr_test
