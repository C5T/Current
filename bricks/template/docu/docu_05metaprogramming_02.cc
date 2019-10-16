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

#include "../../../3rdparty/gtest/gtest-main.h"

  // Map.
  template<typename T> struct add_100 { enum { x = T::x + 100 }; };
  
TEST(TemplateMetaprogramming, Map) {
  struct A { enum { x = 1 }; };
  struct B { enum { x = 2 }; };
  struct C { enum { x = 3 }; };
  
  typedef TypeList<A, B, C> TYPELIST_BEFORE;
  static_assert(TypeListSize<TYPELIST_BEFORE> == 3, "");
  static_assert(TypeListElement<0, TYPELIST_BEFORE>::x == 1, "");
  static_assert(TypeListElement<1, TYPELIST_BEFORE>::x == 2, "");
  static_assert(TypeListElement<2, TYPELIST_BEFORE>::x == 3, "");
  
  typedef current::metaprogramming::map<add_100, TYPELIST_BEFORE> TYPELIST_AFTER;
  static_assert(TypeListSize<TYPELIST_AFTER> == 3, "");
  static_assert(TypeListElement<0, TYPELIST_AFTER>::x == 101, "");
  static_assert(TypeListElement<1, TYPELIST_AFTER>::x == 102, "");
  static_assert(TypeListElement<2, TYPELIST_AFTER>::x == 103, "");
}
  
  // Filter.
  template <typename T> struct y_is_even { enum { filter = ((T::y % 2) == 0) }; };
  
TEST(TemplateMetaprogramming, Filter) {
  struct A { enum { y = 10 }; };
  struct B { enum { y = 15 }; };
  struct C { enum { y = 20 }; };
 
  typedef TypeList<A, B, C> TYPELIST_BEFORE;
  static_assert(TypeListSize<TYPELIST_BEFORE> == 3, "");
  static_assert(TypeListElement<0, TYPELIST_BEFORE>::y == 10, "");
  static_assert(TypeListElement<1, TYPELIST_BEFORE>::y == 15, "");
  static_assert(TypeListElement<2, TYPELIST_BEFORE>::y == 20, "");
  
  typedef current::metaprogramming::filter<y_is_even, TYPELIST_BEFORE> TYPELIST_AFTER;
  static_assert(TypeListSize<TYPELIST_AFTER> == 2, "");
  static_assert(TypeListElement<0, TYPELIST_AFTER>::y == 10, "");
  static_assert(TypeListElement<1, TYPELIST_AFTER>::y == 20, "");
}
    
  // Reduce.
  template<typename A, typename B> struct concatenate_s {
    static std::string s() { return "(" + A::s() + "+" + B::s() + ")"; }
  };
      
TEST(TemplateMetaprogramming, Reduce) {
  struct A { static std::string s() { return "A"; } };
  struct B { static std::string s() { return "B"; } };
  struct C { static std::string s() { return "C"; } };
  EXPECT_EQ("(A+(B+C))", (current::metaprogramming::reduce<concatenate_s, TypeList<A, B, C>>::s()));
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

    // `operator()` on an instance of a `combine`-d type
    // calls `operator()` of the type from the type list that matches
    // the types of parameters passed in.
    // If none or more than one of the input types have the matching signature,
    // attempting such a call will result in compilation error.
    int operator()(TYPE, int a) { return -a; }

    // `DispatchToAll()` on an instance of `combine`-d type
    // calls `DispatchToAll()` from all combined classes,
    // in the order they have been listed in the type list.
    template<typename T> void DispatchToAll(T& x) { x.result += "NEG\n"; }
  };
  
  struct ADD {
    struct TYPE {};
    int operator()(TYPE, int a, int b) { return a + b; }
    // Prove that the method is instantiated at compile time.
    template <typename T>
    int operator()(TYPE, int a, int b, T c) {
      return a + b + AsInt(c);
    }

    template<typename T> void DispatchToAll(T& x) { x.result += "ADD\n"; }
  };
  
  // Since "has-a" is used instead of "is-a",
  // mutual inheritance of underlying types
  // is not a problem at all.
  // Confirm this by making `MUL` inherit from `ADD`.
  struct MUL : ADD {
    struct TYPE {};
    int operator()(TYPE, int a, int b) { return a * b; }
    int operator()(TYPE, int a, int b, int c) { return a * b * c; }

    template<typename T> void DispatchToAll(T& x) { x.result += "MUL\n"; }
  };
    
  // User-friendly method names, internally dispatching calls via `operator()`.
  // A good way to make sure new names appear in one place only, since
  // using `using`-s would require writing them down at least twice each.
  struct UserFriendlyArithmetics :
      current::metaprogramming::combine<TypeList<NEG, ADD, MUL>> {
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
  
  typedef current::metaprogramming::combine<TypeList<NEG, ADD, MUL>> Arithmetics;
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
  
  // RTTI.
namespace rtti_unittest {
  struct BASE {
    // Need a virtual base.
    virtual ~BASE() = default;
  };

  struct A : virtual BASE {
    int a = 100;
    void foo(std::ostringstream& os) {
      ++a;
      os << "mutable a=" << a;
    }
    void foo(std::ostringstream& os) const { os << "const a=" << a; }
  };

  // Inherit from `A` as well, just to show that we can.
  struct B : virtual A, virtual BASE {
    int b = 200;
    void bar(std::ostringstream& os) {
      ++b;
      os << "mutable b=" << b;
    }
    void bar(std::ostringstream& os) const { os << "const b=" << b; }
  };

  // Even more "multiple" inheritance.
  struct C : virtual A, virtual B, virtual BASE {
    int c = 300;
    void baz(std::ostringstream& os) {
      ++c;
      os << "mutable c=" << c;
    }
    void baz(std::ostringstream& os) const { os << "const c=" << c; }
  };
}  // namespace rtti_unittest

TEST(TemplateMetaprogramming, RTTIDynamicCall) {
using namespace rtti_unittest;
  A a;
  B b;
  C c;
  
  const BASE& const_pa = a;
  const BASE& const_pb = b;
  const BASE& const_pc = c;
  
  BASE& mutable_pa = a;
  BASE& mutable_pb = b;
  
  struct call_foo_bar {
    void operator()(const A& a) { a.foo(os); }
    void operator()(A& a) {
      // Mutable version.
      a.foo(os);
    }
    void operator()(const B& b) { b.bar(os); }
    void operator()(B& b) {
      // Mutable version.
      b.bar(os);
    }
    std::ostringstream os;
  };

  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(const_pa, foo_bar);
    EXPECT_EQ("const a=100", foo_bar.os.str());
  }

  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(const_pb, foo_bar);
    EXPECT_EQ("const b=200", foo_bar.os.str());
  }
  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(mutable_pa, foo_bar);
    EXPECT_EQ("mutable a=101", foo_bar.os.str());
  }
  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(mutable_pb, foo_bar);
    EXPECT_EQ("mutable b=201", foo_bar.os.str());
  }
  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(const_pa, foo_bar);
    EXPECT_EQ("const a=101", foo_bar.os.str());
  }
  {
    call_foo_bar foo_bar;
    RTTIDynamicCall<TypeList<A, B>>(const_pb, foo_bar);
    EXPECT_EQ("const b=201", foo_bar.os.str());
  }

  struct call_bar_baz {
    void operator()(const B& b) { b.bar(os); }
    void operator()(const C& c) { c.baz(os); }
    std::ostringstream os;
  };

  {
    call_bar_baz bar_baz;
    RTTIDynamicCall<TypeList<B, C>>(const_pb, bar_baz);
    EXPECT_EQ("const b=201", bar_baz.os.str());
  }
  {
    call_bar_baz bar_baz;
    RTTIDynamicCall<TypeList<B, C>>(const_pc, bar_baz);
    EXPECT_EQ("const c=300", bar_baz.os.str());
  }

  struct call_foo_baz {
    void operator()(const A& a) { a.foo(os); }
    void operator()(A& a) {
      // Mutable version.
      a.foo(os);
    }
    void operator()(const C& c) { c.baz(os); }
    void operator()(C& c) {
      // Mutable version.
      c.baz(os);
    }
    void operator()(const A& a, int x, const std::string& y) {
      a.foo(os);
      os << ", [" << x << "]['" << y << "']";
    }
    void operator()(A& a, int x, const std::string& y) {
      // Mutable version.
      a.foo(os);
      os << ", [" << x << "]['" << y << "']";
    }
    void operator()(const C& c, int x, const std::string& y) {
      c.baz(os);
      os << ", [" << x << "]['" << y << "']";
    }
    void operator()(C& c, int x, const std::string& y) {
      // Mutable version.
      c.baz(os);
      os << ", [" << x << "]['" << y << "']";
    }
    std::ostringstream os;
  };

  std::unique_ptr<BASE> unique_a(new A());
  std::unique_ptr<BASE> unique_c(new C());
  BASE* ptr_a = &*unique_a;
  BASE* ptr_c = &*unique_c;
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_a, foo_baz);
    EXPECT_EQ("mutable a=101", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_c, foo_baz);
    EXPECT_EQ("mutable c=301", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*static_cast<const std::unique_ptr<BASE>&>(unique_a), foo_baz);
    EXPECT_EQ("mutable a=102", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*static_cast<const std::unique_ptr<BASE>&>(unique_c), foo_baz);
    EXPECT_EQ("mutable c=302", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_a, foo_baz);
    EXPECT_EQ("mutable a=103", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_c, foo_baz);
    EXPECT_EQ("mutable c=303", foo_baz.os.str());
  }

  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_a, foo_baz, 1, std::string("one"));
    EXPECT_EQ("mutable a=104, [1]['one']", foo_baz.os.str());
  }
  {
    call_foo_baz foo_baz;
    RTTIDynamicCall<TypeList<A, C>>(*ptr_c, foo_baz, 2, std::string("two"));
    EXPECT_EQ("mutable c=304, [2]['two']", foo_baz.os.str());
  }

  struct call_foo_bar_by_rvalue_reference {
    void operator()(A&& a) { a.foo(os); }
    void operator()(B&& b) { b.bar(os); }
    std::ostringstream os;
  };

  {
    call_foo_bar_by_rvalue_reference foo_bar;
    A a;
    RTTIDynamicCall<TypeList<A, B>>(std::move(a), foo_bar);
    EXPECT_EQ("mutable a=101", foo_bar.os.str());
  }

  {
    call_foo_bar foo_bar_1;
    call_foo_bar_by_rvalue_reference foo_bar_2;
    B b;
    RTTIDynamicCall<TypeList<A, B>>(b, foo_bar_1);
    RTTIDynamicCall<TypeList<A, B>>(std::move(b), foo_bar_2);
    EXPECT_EQ("mutable b=201", foo_bar_1.os.str());
    EXPECT_EQ("mutable b=202", foo_bar_2.os.str());
  }

// TODO(dkorolev): Test all the corner cases with exceptions, wrong base types, etc.
}
