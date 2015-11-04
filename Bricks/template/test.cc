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

  bricks::metaprogramming::map<add_100, decltype(before)> after;
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

  bricks::metaprogramming::filter<y_is_even, decltype(before)> after;
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
  EXPECT_EQ("(A+(B+C))", (bricks::metaprogramming::reduce<concatenate_s, std::tuple<A, B, C>>::s()));
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

  typedef bricks::metaprogramming::combine<std::tuple<NEG, ADD, MUL>> Arithmetics;
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
  using bricks::variadic_indexes::indexes;
  using bricks::variadic_indexes::generate_indexes;

  static_assert(std::is_same<indexes<>, generate_indexes<0>>::value, "");
  static_assert(std::is_same<indexes<0>, generate_indexes<1>>::value, "");
  static_assert(std::is_same<indexes<0,1>, generate_indexes<2>>::value, "");
  static_assert(std::is_same<indexes<0,1,2>, generate_indexes<3>>::value, "");
}
