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
    // is to use a helper local type as the 1st param in the signature.
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
  
  // RTTI.
namespace rtti_unittest {
  struct BASE {
    // Need a virtual base.
    virtual ~BASE() = default;
  };
  
  struct A : virtual BASE {
    int a = 101;
    void foo(std::ostream& os) {
      os << "a=" << a << std::endl;
    }
  };
  
  // Inherit from `A` as well, just to show that we can.
  struct B : virtual A, virtual BASE {
    int b = 102;
    void bar(std::ostream& os) {
      os << "b=" << b << std::endl;
    }
  };

  // Even more "multiple" inheritance.
  struct C : virtual A, virtual B, virtual BASE {
    int c = 103;
    void baz(std::ostream& os) {
      os << "c=" << c << std::endl;
    }
  };
}  // namespace rtti_unittest

TEST(TemplateMetaprogramming, RTTIDynamicCall) {
using namespace rtti_unittest;
using bricks::metaprogramming::RTTIDynamicCall;
  
  A a;
  B b;
  C c;
  
  BASE& pa = a;
  BASE& pb = b;
  BASE& pc = c;
  
  struct call_foo_bar {
    void operator()(A& a) {
      a.foo(os);
    }
    void operator()(B& b) {
      b.bar(os);
    }
    std::ostringstream os;
  } foo_bar;
  
  RTTIDynamicCall<std::tuple<A, B>>(pa, foo_bar);
  RTTIDynamicCall<std::tuple<A, B>>(pb, foo_bar);
  EXPECT_EQ("a=101\nb=102\n", foo_bar.os.str());
  
  struct call_bar_baz {
    void operator()(B& b) {
      b.bar(os);
    }
    void operator()(C& c) {
      c.baz(os);
    }
    std::ostringstream os;
  } bar_baz;
  
  RTTIDynamicCall<std::tuple<B, C>>(pb, bar_baz);
  RTTIDynamicCall<std::tuple<B, C>>(pc, bar_baz);
  EXPECT_EQ("b=102\nc=103\n", bar_baz.os.str());

  struct call_foo_baz {
    void operator()(A& a) {
      a.foo(os);
    }
    void operator()(C& c) {
      c.baz(os);
    }
    std::ostringstream os;
  } foo_baz;
  
  std::unique_ptr<BASE> unique_a(new A());
  std::unique_ptr<BASE> unique_c(new C());
  RTTIDynamicCall<std::tuple<A, C>>(unique_a, foo_baz);
  RTTIDynamicCall<std::tuple<A, C>>(unique_c, foo_baz);
  EXPECT_EQ("a=101\nc=103\n", foo_baz.os.str());

// TODO(dkorolev): Test all the corner cases with exceptions, wrong base types, etc.
}
