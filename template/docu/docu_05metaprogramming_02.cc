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

#include <string>
#include <sstream>

#include "../metaprogramming.h"

#include "../../3party/gtest/gtest-main.h"

  // Map.
  template<typename T> struct add_100 { enum { x = T::x + 100 }; };
  
TEST(TemplateMetaprogramming, Map) {
  struct A { enum { x = 1 }; };
  struct B { enum { x = 2 }; };
  struct C { enum { x = 3 }; };
  
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
  
  // Filter.
  template <typename T> struct y_is_even { enum { filter = ((T::y % 2) == 0) }; };
  
TEST(TemplateMetaprogramming, Filter) {
  struct A { enum { y = 10 }; };
  struct B { enum { y = 15 }; };
  struct C { enum { y = 20 }; };
  
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
    
  // Reduce.
  template<typename A, typename B> struct concatenate_s {
    static std::string s() { return "(" + A::s() + "+" + B::s() + ")"; }
  };
      
TEST(TemplateMetaprogramming, Reduce) {
  struct A { static std::string s() { return "A"; } };
  struct B { static std::string s() { return "B"; } };
  struct C { static std::string s() { return "C"; } };
  EXPECT_EQ("(A+(B+C))",
            (bricks::metaprogramming::reduce<concatenate_s, std::tuple<A, B, C>>::s()));
}
    
template <typename T> struct AsIntImpl {};
template <> struct AsIntImpl<int> { static int DoIt(int x) { return x; } };
template <> struct AsIntImpl<const char*> { static int DoIt(const char* s) { return atoi(s); } };
template <typename T> int AsInt(T x) { return AsIntImpl<T>::DoIt(x); }

  // Combine.
  struct NEG {
    // A simple way to differentiate logic by struct/class type
    // is to provide a local, unique, symbol as the 1st param.
    struct TYPE {};

    // Combine-able operations are defined as `operator()`.
    // Just because we need to pick one common name,
    // otherwise more `using`-s will be needed.
    int operator()(TYPE, int a) { return -a; }
  };
  
  struct ADD {
    struct TYPE {};
    int operator()(TYPE, int a, int b) { return a + b; }
    // Prove that the method is instantiated at compile time.
    template <typename T>
    int operator()(TYPE, int a, int b, T c) {
      return a + b + AsInt(c);
    }
  };
  
  // Since "has-a" is used instead of "is-a",
  // mutual inheritance of underlying types
  // is not a problem at all.
  // Confirm this by making `MUL` inherit from `ADD`.
  struct MUL : ADD {
    struct TYPE {};
    int operator()(TYPE, int a, int b) { return a * b; }
    int operator()(TYPE, int a, int b, int c) { return a * b * c; }
  };
    
  // User-friendly method names, internally dispatching calls via `operator()`.
  // A good way to make sure new names appear in one place only, since
  // using `using`-s would require writing them down at least twice each.
  struct UserFriendlyArithmetics :
      bricks::metaprogramming::combine<std::tuple<NEG, ADD, MUL>> {
    int Neg(int x) {
      return operator()(NEG::TYPE(), x);
    }
    template <typename... T>
    int Add(T... xs) {
      return operator()(ADD::TYPE(), xs...);
    }
    template <typename... T>
    int Mul(T... xs) {
      return operator()(MUL::TYPE(), xs...);
    }
    // The implementation for `Div()` is not provided,
    // yet the code will compile until it's attempted to be used.
    // (Unit tests and code coverage measurement FTW!)
    template <typename... T>
    int Div(T... xs) {
      struct TypeForWhichThereIsNoImplemenation {};
      return operator()(TypeForWhichThereIsNoImplemenation(),
                        xs...);
    }
  };

TEST(TemplateMetaprogramming, Combine) {
  EXPECT_EQ(1, NEG()(NEG::TYPE(), -1));
  EXPECT_EQ(3, ADD()(ADD::TYPE(), 1, 2));
  EXPECT_EQ(6, ADD()(ADD::TYPE(), 1, 2, "3"));
  EXPECT_EQ(20, MUL()(MUL::TYPE(), 4, 5));
  EXPECT_EQ(120, MUL()(MUL::TYPE(), 4, 5, 6));
  
  // As a sanity check, since `MUL` inherits from `ADD`,
  // the following construct will work just fine.
  EXPECT_EQ(15, MUL().ADD::operator()(ADD::TYPE(), 7, 8));
  
  // Using the simple combiner, that still uses `operator()`.
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
}

#if 0
  // Visitor.
namespace visitor_unittest {
using bricks::metaprogramming::visitor;
using bricks::metaprogramming::visitable;
using bricks::metaprogramming::abstract_visitable;

  using typelist_ab = std::tuple<struct A, struct B>;
  using typelist_bc = std::tuple<struct B, struct C>;
    
  struct A : visitable<typelist_ab, A> {
    int a = 101;
    void foo(std::ostream& os) {
      os << "a=" << a << std::endl;
    }
  };
  
  struct B : visitable<typelist_ab, B>, visitable<typelist_bc, B> {
    int b = 102;
    void bar(std::ostream& os) {
      os << "b=" << b << std::endl;
    }
  };
  
  struct C : visitable<typelist_bc, C> {
    int c = 103;
    void baz(std::ostream& os) {
      os << "c=" << c << std::endl;
    }
  };
}  // namespace visitor_unittest
  
TEST(TemplateMetaprogramming, Visitor) {
using namespace visitor_unittest;

  A a;
  B b;
  C c;
  
  struct call_foo_bar : visitor<typelist_ab> {
    // Note that forgetting to handle one of `visit()` overrides will result in compile errors of two types:
    // 1) overriding what is not `virtual`, thus attempting to operate on a parameter not from the type list, or
    // 2) not overriding what should be overridden, thus attempting to instantiate the `visitor` that is abstract.
    virtual void visit(A& a) override {
      a.foo(os);
    }
    virtual void visit(B& b) override {
      b.bar(os);
    }
    std::ostringstream os;
  } foo_bar;

  for (auto& it : std::vector<abstract_visitable<typelist_ab>*>({ &a, &b })) {
    it->accept(foo_bar);
  }
  EXPECT_EQ("a=101\nb=102\n", foo_bar.os.str());
  
  struct call_bar_baz : visitor<typelist_bc> {
    virtual void visit(B& b) override {
      b.bar(os);
    }
    virtual void visit(C& c) override {
      c.baz(os);
    }
    std::ostringstream os;
  } bar_baz;

  for (auto& it : std::vector<abstract_visitable<typelist_bc>*>({ &b, &c })) {
    it->accept(bar_baz);
  }
  EXPECT_EQ("b=102\nc=103\n", bar_baz.os.str());
}
#endif
