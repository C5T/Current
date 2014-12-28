// TODO(dkorolev): Add a test that throws UnrecognizedPolymorphicTypeException.

#include "dispatcher.h"

#include <string>
#include <tuple>

#include "../3party/gtest/gtest.h"
#include "../3party/gtest/gtest-main.h"

using std::string;
using std::tuple;

template <typename... TYPES>
struct TypeList;

// Empty constructors required by clang++.
struct Base {
  Base() {}
  // Need to define at least one virtual method.
  virtual ~Base() = default;
};
struct Foo : Base {
  Foo() {}
};
struct Bar : Base {
  Bar() {}
};
struct Baz : Base {
  Baz() {}
};

typedef TypeList<Foo, Bar, Baz> FooBarBazTypeList;

struct Processor {
  string s;
  void operator()(const Base&) { s = "const Base&"; }
  void operator()(const Foo&) { s = "const Foo&"; }
  void operator()(const Bar&) { s = "const Bar&"; }
  void operator()(const Baz&) { s = "const Baz&"; }
  void operator()(Base&) { s = "Base&"; }
  void operator()(Foo&) { s = "Foo&"; }
  void operator()(Bar&) { s = "Bar&"; }
  void operator()(Baz&) { s = "Baz&"; }
};

TEST(RuntimeDispatcher, StaticCalls) {
  Processor p;
  EXPECT_EQ("", p.s);
  p(Base());
  EXPECT_EQ("const Base&", p.s);
  p(Foo());
  EXPECT_EQ("const Foo&", p.s);
  p(Bar());
  EXPECT_EQ("const Bar&", p.s);
  p(Baz());
  EXPECT_EQ("const Baz&", p.s);
}

TEST(RuntimeDispatcher, ImmutableStaticCalls) {
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  Processor p;
  EXPECT_EQ("", p.s);
  p(base);
  EXPECT_EQ("const Base&", p.s);
  p(foo);
  EXPECT_EQ("const Foo&", p.s);
  p(bar);
  EXPECT_EQ("const Bar&", p.s);
  p(baz);
  EXPECT_EQ("const Baz&", p.s);
}

TEST(RuntimeDispatcher, MutableStaticCalls) {
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  Processor p;
  EXPECT_EQ("", p.s);
  p(base);
  EXPECT_EQ("Base&", p.s);
  p(foo);
  EXPECT_EQ("Foo&", p.s);
  p(bar);
  EXPECT_EQ("Bar&", p.s);
  p(baz);
  EXPECT_EQ("Baz&", p.s);
}

TEST(RuntimeDispatcher, ImmutableWithoutDispatching) {
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  p(rbase);
  EXPECT_EQ("const Base&", p.s);
  p(rfoo);
  EXPECT_EQ("const Base&", p.s);
  p(rbar);
  EXPECT_EQ("const Base&", p.s);
  p(rbaz);
  EXPECT_EQ("const Base&", p.s);
}

TEST(RuntimeDispatcher, MutableWithoutDispatching) {
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  p(rbase);
  EXPECT_EQ("Base&", p.s);
  p(rfoo);
  EXPECT_EQ("Base&", p.s);
  p(rbar);
  EXPECT_EQ("Base&", p.s);
  p(rbaz);
  EXPECT_EQ("Base&", p.s);
}

TEST(RuntimeDispatcher, ImmutableWithDispatching) {
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbase, p);
  EXPECT_EQ("const Base&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rfoo, p);
  EXPECT_EQ("const Foo&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbar, p);
  EXPECT_EQ("const Bar&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbaz, p);
  EXPECT_EQ("const Baz&", p.s);
}

TEST(RuntimeDispatcher, MutableWithDispatching) {
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbase, p);
  EXPECT_EQ("Base&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rfoo, p);
  EXPECT_EQ("Foo&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbar, p);
  EXPECT_EQ("Bar&", p.s);
  bricks::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbaz, p);
  EXPECT_EQ("Baz&", p.s);
}

TEST(RuntimeDispatcher, ImmutableWithTupleTypeListDispatching) {
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbase, p);
  EXPECT_EQ("const Base&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rfoo, p);
  EXPECT_EQ("const Foo&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbar, p);
  EXPECT_EQ("const Bar&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbaz, p);
  EXPECT_EQ("const Baz&", p.s);
}

TEST(RuntimeDispatcher, MutableWithTupleTypeListDispatching) {
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  Processor p;
  EXPECT_EQ("", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbase, p);
  EXPECT_EQ("Base&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rfoo, p);
  EXPECT_EQ("Foo&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbar, p);
  EXPECT_EQ("Bar&", p.s);
  bricks::rtti::RuntimeTupleDispatcher<Base, tuple<Foo, Bar, Baz>>::DispatchCall(rbaz, p);
  EXPECT_EQ("Baz&", p.s);
}
