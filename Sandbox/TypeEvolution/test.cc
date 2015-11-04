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

#include <string>

#include "../../Bricks/template/mapreduce.h"
#include "../../Bricks/template/combine.h"
#include "../../Bricks/template/weed.h"
#include "../../Bricks/template/typelist.h"

#include "../../3rdparty/gtest/gtest-main.h"

// `AcceptTypes<TypeList<...>>` is a helper type that declares `operator()` for each type from the type list.
template<typename T>
struct AcceptType {
  // TODO(dkorolev): This test should compile and run with definition only, not declaration.
  std::string operator()(const T&) const { return "foo"; }
};

template<typename T>
using AcceptTypes = bricks::metaprogramming::combine<bricks::metaprogramming::map<AcceptType, T>>;

// `AcceptsAllTypes<F, TypeList<...>>::value` is true iff `F` can accept any type from the type list.
template<typename F, typename T>
struct AcceptsAllTypes {
  template<typename X>
  struct Accepts {
    enum { filter = bricks::weed::call_with<F, X>::implemented };
  };
  enum { value = std::is_same<T, bricks::metaprogramming::filter<Accepts, T>>::value };
};

// `Converter` is the middle layer powering type evolution.
// It enables seamless conversion of types from one schema into some other one.

template<typename T, typename F>
struct ConverterImpl {
  T t;
  const F& f;
  ConverterImpl(const F& f) : f(f) {}
  // TODO(dkorolev): Should certainly not be `std::string`.
  // TODO(dkorolev): Should certainly not be `const`.
  template<typename X> std::string operator()(X&& x) const {
    return t(f, std::forward<X>(x));
  }
};

template<typename T, typename F>
ConverterImpl<T, F> Converter(const F& f) {
  return ConverterImpl<T, F>(f);
}

struct PassThrough {
  // TODO(dkorolev): Should certainly not be `std::string`.
  // TODO(dkorolev): Should certainly not be `const`.
  template<typename F, typename X>
  std::string operator()(F&& f, X&& x) const {
    return f(std::forward<X>(x));
  }
};

namespace type_evolution_test {
struct A {};
struct B {};
struct C {};
struct Z {};

struct ChangeCIntoB {
  // TODO(dkorolev): Should certainly not be `std::string`.
  // TODO(dkorolev): Should certainly not be `const`.
  template<typename F, typename X>
  std::string operator()(F&& f, X&& x) const {
    return f(std::forward<X>(x));
  }
  template<typename F>
  std::string operator()(F&& f, C) const {
    return f(B());
  }
};

// `SomeUserLogic` accepts only `Z`, and emits `A, B, C`.
struct SomeUserLogic {
  template<typename F>
  void operator()(F&& f, Z) const {
    // Emit all { A, B, C }. They might be guarded by if-s or something, this is just the semantical test.
    f(A());
    f(B());
    f(C());
  }
};
}  // namespace type_evolution_test

TEST(TypeEvolution, SmokeTest) {
  using namespace type_evolution_test;

  // Confirm that `AcceptTypes` does its job.
  using AcceptsAB = AcceptTypes<TypeList<A, B>>;
  static_assert(bricks::weed::call_with<AcceptsAB, A>::implemented == true, "");
  static_assert(bricks::weed::call_with<AcceptsAB, B>::implemented == true, "");
  static_assert(bricks::weed::call_with<AcceptsAB, C>::implemented == false, "");

  // Confirm that `AcceptsAllTypes` does its job.
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<>>::value == true, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<A>>::value == true, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<B>>::value == true, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<A, B>>::value == true, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<C>>::value == false, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<A, C>>::value == false, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<B, C>>::value == false, "");
  static_assert(AcceptsAllTypes<AcceptsAB, TypeList<A, B, C>>::value == false, "");

  // Simple type semantics verifier.
  struct TypeChecker {
    // TODO(dkorolev): Should work w/o `const`.
    std::string operator()(A) const { return "A"; }
    std::string operator()(B) const { return "B"; }
    std::string operator()(C) const { return "C"; }
  };

  TypeChecker checker;
  EXPECT_EQ("A", checker(A()));
  EXPECT_EQ("B", checker(B()));
  EXPECT_EQ("C", checker(C()));

  // Confirm that `Converter` does its job.
  const auto passthrough = Converter<PassThrough>(checker);
  EXPECT_EQ("A", passthrough(A()));
  EXPECT_EQ("B", passthrough(B()));
  EXPECT_EQ("C", passthrough(C()));

  const auto change_c_into_b = Converter<ChangeCIntoB>(checker);
  EXPECT_EQ("A", change_c_into_b(A()));
  EXPECT_EQ("B", change_c_into_b(B()));
  EXPECT_EQ("B", change_c_into_b(C()));

  // TODO(dkorolev): Compile-time complete schema conversion validation.
  static_assert(bricks::weed::call_with<AcceptTypes<TypeList<A, B>>, A>::implemented, "");

  SomeUserLogic()(AcceptTypes<TypeList<A, B, C>>(), Z());
  // SomeUserLogic()(AcceptTypes<TypeList<A, B>>(), Z());  // Does not compile, since `SomeUserLogic(Z)` emits `C` as well.
  // WTF?
  // SomeUserLogic()(Converter<ChangeCIntoB>(AcceptTypes<TypeList<A, B>>()), Z());

  AcceptTypes<TypeList<A, B>>()((A()));
  AcceptTypes<TypeList<A, B>>()((B()));
  // AcceptTypes<TypeList<A, B>>()((C()));  // Does not compile.

  // TODO(dkorolev): Why doesn't `const` work here?
  auto inner_acceptor = AcceptTypes<TypeList<A, B>>();
  // auto outer_acceptor = Converter<ChangeCIntoB>(inner_acceptor);
  auto outer_acceptor = Converter<PassThrough>(inner_acceptor);
  inner_acceptor(A());
  inner_acceptor(B());
  // inner_acceptor(C());  // Does not compile.

  // TODO(dkorolev): This does not compile yet.
  static_cast<void>(outer_acceptor);
  // outer_acceptor(A());
  // outer_acceptor(B());
  // outer_acceptor(C());  // Should compile fine.

  // TODO(dkorolev): A cover call that would try all the types from an input typelist.
}
