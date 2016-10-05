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

#include "docu/docu_06rtti_01_test.cc"

#include "dispatcher.h"

#include <string>

#include "../../3rdparty/gtest/gtest-main.h"

namespace rtti_unittest {

struct Base {
  // Empty constructor required by clang++.
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

struct OtherBase {
  // Empty constructor required by clang++.
  OtherBase() {}
  // Need to define at least one virtual method.
  virtual ~OtherBase() = default;
};

struct RTTITestProcessor {
  std::string s;
  void operator()(const Base&) { s = "const Base&"; }
  void operator()(const Foo&) { s = "const Foo&"; }
  void operator()(const Bar&) { s = "const Bar&"; }
  void operator()(const Baz&) { s = "const Baz&"; }
  void operator()(Base&) { s = "Base&"; }
  void operator()(Foo&) { s = "Foo&"; }
  void operator()(Bar&) { s = "Bar&"; }
  void operator()(Baz&) { s = "Baz&"; }
  void operator()(Base&&) { s = "Base&&"; }
  void operator()(Foo&&) { s = "Foo&&"; }
  void operator()(Bar&&) { s = "Bar&&"; }
  void operator()(Baz&&) { s = "Baz&&"; }
};

}  // namespace rtti_unittest

TEST(RuntimeDispatcher, StaticCalls) {
  using namespace rtti_unittest;
  RTTITestProcessor p;
  EXPECT_EQ("", p.s);
  p(Base());
  EXPECT_EQ("Base&&", p.s);
  p(Foo());
  EXPECT_EQ("Foo&&", p.s);
  p(Bar());
  EXPECT_EQ("Bar&&", p.s);
  p(Baz());
  EXPECT_EQ("Baz&&", p.s);
}

TEST(RuntimeDispatcher, ImmutableStaticCalls) {
  using namespace rtti_unittest;
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  RTTITestProcessor p;
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
  using namespace rtti_unittest;
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  RTTITestProcessor p;
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
  using namespace rtti_unittest;
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  RTTITestProcessor p;
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
  using namespace rtti_unittest;
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  RTTITestProcessor p;
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
  using namespace rtti_unittest;
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const OtherBase other;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  const OtherBase& rother = other;
  RTTITestProcessor p;
  EXPECT_EQ("", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbase, p);
  EXPECT_EQ("const Base&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rfoo, p);
  EXPECT_EQ("const Foo&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbar, p);
  EXPECT_EQ("const Bar&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbaz, p);
  EXPECT_EQ("const Baz&", p.s);
  ASSERT_THROW((current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rother, p)),
               current::rtti::UnrecognizedPolymorphicType);
}

TEST(RuntimeDispatcher, MutableWithDispatching) {
  using namespace rtti_unittest;
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  OtherBase other;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  OtherBase& rother = other;
  RTTITestProcessor p;
  EXPECT_EQ("", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbase, p);
  EXPECT_EQ("Base&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rfoo, p);
  EXPECT_EQ("Foo&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbar, p);
  EXPECT_EQ("Bar&", p.s);
  current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rbaz, p);
  EXPECT_EQ("Baz&", p.s);
  ASSERT_THROW((current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rother, p)),
               current::rtti::UnrecognizedPolymorphicType);
}

TEST(RuntimeDispatcher, ImmutableWithTypeListTypeListDispatching) {
  using namespace rtti_unittest;
  const Base base;
  const Foo foo;
  const Bar bar;
  const Baz baz;
  const OtherBase other;
  const Base& rbase = base;
  const Base& rfoo = foo;
  const Base& rbar = bar;
  const Base& rbaz = baz;
  const OtherBase& rother = other;
  RTTITestProcessor p;
  EXPECT_EQ("", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<>>::DispatchCall(rbase, p);
  EXPECT_EQ("const Base&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbase, p);
  EXPECT_EQ("const Base&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rfoo, p);
  EXPECT_EQ("const Foo&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbar, p);
  EXPECT_EQ("const Bar&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbaz, p);
  EXPECT_EQ("const Baz&", p.s);
  ASSERT_THROW((current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rother, p)),
               current::rtti::UnrecognizedPolymorphicType);
}

TEST(RuntimeDispatcher, MutableWithTypeListTypeListDispatching) {
  using namespace rtti_unittest;
  Base base;
  Foo foo;
  Bar bar;
  Baz baz;
  OtherBase other;
  Base& rbase = base;
  Base& rfoo = foo;
  Base& rbar = bar;
  Base& rbaz = baz;
  OtherBase& rother = other;
  RTTITestProcessor p;
  EXPECT_EQ("", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<>>::DispatchCall(rbase, p);
  EXPECT_EQ("Base&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbase, p);
  EXPECT_EQ("Base&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rfoo, p);
  EXPECT_EQ("Foo&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbar, p);
  EXPECT_EQ("Bar&", p.s);
  current::rtti::RuntimeTypeListDispatcher<Base, TypeListImpl<Foo, Bar, Baz>>::DispatchCall(rbaz, p);
  EXPECT_EQ("Baz&", p.s);
  ASSERT_THROW((current::rtti::RuntimeDispatcher<Base, Foo, Bar, Baz>::DispatchCall(rother, p)),
               current::rtti::UnrecognizedPolymorphicType);
}

TEST(RuntimeDispatcher, MoveSemantics) {
  using namespace rtti_unittest;
  {
    Foo foo;
    const Base& x = foo;
    RTTITestProcessor p;
    EXPECT_EQ("", p.s);
    current::rtti::RuntimeDispatcher<Base, Foo>::DispatchCall(x, p);
    EXPECT_EQ("const Foo&", p.s);
  }
  {
    Bar bar;
    Base&& y = std::move(bar);
    RTTITestProcessor p;
    EXPECT_EQ("", p.s);
    current::rtti::RuntimeDispatcher<Base, Bar>::DispatchCall(std::move(y), p);
    EXPECT_EQ("Bar&&", p.s);
  }}
