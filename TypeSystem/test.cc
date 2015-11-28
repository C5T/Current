/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "enum.h"
#include "struct.h"
#include "optional.h"
#include "polymorphic.h"
#include "timestamp.h"

#include "../3rdparty/gtest/gtest-main.h"

#include "Reflection/test.cc"
#include "Serialization/test.cc"

namespace struct_definition_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) {
  CURRENT_FIELD(i, uint64_t, 0u);
  CURRENT_CONSTRUCTOR(Foo)(uint64_t i = 42u) : i(i) {}
};
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(j, uint64_t, 0u);
  CURRENT_CONSTRUCTOR(Bar)(uint64_t j = 43u) : j(j) {}
};
CURRENT_STRUCT(Baz) {
  CURRENT_FIELD(v1, std::vector<uint64_t>);
  CURRENT_FIELD(v2, std::vector<Foo>);
  CURRENT_FIELD(v3, std::vector<std::vector<Foo>>);
  CURRENT_FIELD(v4, (std::map<std::string, std::string>));
  CURRENT_FIELD(v5, (std::map<Foo, int>));
};
CURRENT_STRUCT(DerivedFromFoo, Foo) { CURRENT_FIELD(bar, Baz); };

static_assert(IS_VALID_CURRENT_STRUCT(Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Baz), "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(DerivedFromFoo), "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace struct_definition_test

// Confirm that Current data types defined in a namespace are accessible from outside it.
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Baz), "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

namespace some_other_namespace {

// Confirm that Current data types defined in one namespace are accessible from another one.
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Foo),
              "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Baz),
              "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace some_other_namespace

namespace struct_definition_test {

// Properly declared structures.
CURRENT_STRUCT(Empty){};
CURRENT_STRUCT(EmptyDerived, Empty){};

static_assert(IS_VALID_CURRENT_STRUCT(Empty), "`Empty` must pass `IS_VALID_CURRENT_STRUCT` check.");
static_assert(IS_VALID_CURRENT_STRUCT(EmptyDerived),
              "`EmptyDerived` must pass `IS_VALID_CURRENT_STRUCT` check.");

// Improperly declared structures.
struct WrongStructNotCurrentStruct {
  int x;
};
struct WrongDerivedStructNotCurrentStruct : ::current::CurrentSuper {};
struct NotCurrentStructDerivedFromCurrentStruct : Empty {};

CURRENT_STRUCT(WrongUsesCOUNTERInternally) {
  CURRENT_FIELD(i1, uint64_t);
  static size_t GetCounter() { return __COUNTER__; }
  CURRENT_FIELD(i2, uint64_t);
};

// The lines below don't compile with various errors.
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongStructNotCurrentStruct),
//               "`WrongStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongDerivedStructNotCurrentStruct),
//               "`WrongDerivedStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(NotCurrentStructDerivedFromCurrentStruct),
//               "`NotCurrentStructDerivedFromCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongUsesCOUNTERInternally),
//               "`WrongUsesCOUNTERInternally` must not pass `IS_VALID_CURRENT_STRUCT` check.");

}  // namespace struct_definition_test

TEST(TypeSystemTest, ExistsAndValueSemantics) {
  {
    int x = 1;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(1, Value(x));
    EXPECT_EQ(1, Value(Value(x)));
  }
  {
    const int x = 2;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(2, Value(x));
    EXPECT_EQ(2, Value(Value(x)));
  }
  {
    int y = 3;
    int& x = y;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(3, Value(x));
    EXPECT_EQ(3, Value(Value(x)));
    y = 4;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(4, Value(x));
    EXPECT_EQ(4, Value(Value(x)));
  }
  {
    int y = 5;
    const int& x = y;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(5, Value(x));
    EXPECT_EQ(5, Value(Value(x)));
    y = 6;
    ASSERT_TRUE(Exists(x));
    EXPECT_EQ(6, Value(x));
    EXPECT_EQ(6, Value(Value(x)));
  }
}

TEST(TypeSystemTest, CopyDoesItsJob) {
  using namespace struct_definition_test;

  Foo a;
  Foo b;

  a.i = 1u;
  b.i = 2u;
  EXPECT_EQ(1u, a.i);
  EXPECT_EQ(2u, b.i);

  a.i = 3u;
  EXPECT_EQ(3u, a.i);
  EXPECT_EQ(2u, b.i);

  b = a;
  EXPECT_EQ(3u, a.i);
  EXPECT_EQ(3u, b.i);
}

TEST(TypeSystemTest, ImmutableOptional) {
  {
    ImmutableOptional<int> foo(make_unique<int>(100));
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(100, foo.Value());
    EXPECT_EQ(100, static_cast<int>(Value(foo)));
    EXPECT_EQ(100, static_cast<int>(Value(Value(foo))));
  }
  {
    ImmutableOptional<int> bar(nullptr);
    ASSERT_FALSE(Exists(bar));
    try {
      Value(bar);
      ASSERT_TRUE(false);
    } catch (NoValue) {
    }
  }
  {
    ImmutableOptional<int> meh(nullptr);
    ASSERT_FALSE(Exists(meh));
    try {
      Value(meh);
      ASSERT_TRUE(false);
    } catch (NoValueOfType<int>) {
    }
  }
  {
    struct_definition_test::Foo bare;
    ASSERT_TRUE(Exists(bare));
    EXPECT_EQ(42u, Value(bare).i);
    EXPECT_EQ(42u, Value(Value(bare)).i);
  }
  {
    struct_definition_test::Foo bare;
    ImmutableOptional<struct_definition_test::Foo> wrapped(FromBarePointer(), &bare);
    ASSERT_TRUE(Exists(wrapped));
    EXPECT_EQ(42u, wrapped.Value().i);
    EXPECT_EQ(42u, Value(wrapped).i);
    EXPECT_EQ(42u, Value(Value(wrapped)).i);
  }
  {
    struct_definition_test::Foo bare;
    ImmutableOptional<struct_definition_test::Foo> wrapped(FromBarePointer(), &bare);
    const ImmutableOptional<struct_definition_test::Foo>& double_wrapped(wrapped);
    ASSERT_TRUE(Exists(double_wrapped));
    EXPECT_EQ(42u, double_wrapped.Value().i);
    EXPECT_EQ(42u, Value(double_wrapped).i);
    EXPECT_EQ(42u, Value(Value(double_wrapped)).i);
  }
  {
    const auto lambda =
        [](int x) -> ImmutableOptional<int> { return ImmutableOptional<int>(make_unique<int>(x)); };
    ASSERT_TRUE(Exists(lambda(101)));
    EXPECT_EQ(102, lambda(102).Value());
    EXPECT_EQ(102, Value(lambda(102)));
  }
}

TEST(TypeSystemTest, Optional) {
  Optional<int> foo(make_unique<int>(200));
  ASSERT_TRUE(Exists(foo));
  EXPECT_EQ(200, foo.Value());
  EXPECT_EQ(200, Value(foo));
  foo = nullptr;
  ASSERT_FALSE(Exists(foo));
  try {
    Value(foo);
    ASSERT_TRUE(false);
  } catch (NoValue) {
  }
}

namespace enum_class_test {
CURRENT_ENUM(Fruits, uint32_t){APPLE = 1u, ORANGE = 2u};
}  // namespace enum_class_test

TEST(TypeSystemTest, EnumRegistration) {
  using current::reflection::EnumName;
  EXPECT_EQ("Fruits", EnumName<enum_class_test::Fruits>());
}

TEST(TypeSystemTest, PolymorphicStaticAsserts) {
  using namespace struct_definition_test;

  static_assert(std::is_same<Polymorphic<Foo>, Polymorphic<Foo>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo>, Polymorphic<TypeList<Foo>>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo>, Polymorphic<TypeList<Foo, Foo>>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo>, Polymorphic<TypeListImpl<Foo>>>::value, "");
  static_assert(Polymorphic<Foo>::T_TYPELIST_SIZE == 1u, "");
  static_assert(std::is_same<Polymorphic<Foo>::T_TYPELIST, TypeListImpl<Foo>>::value, "");

  static_assert(std::is_same<Polymorphic<Foo, Bar>, Polymorphic<Foo, Bar>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo, Bar>, Polymorphic<TypeList<Foo, Bar>>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo, Bar>, Polymorphic<TypeList<Foo, Bar, Foo>>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo, Bar>, Polymorphic<TypeList<Foo, Bar, TypeList<Bar>>>>::value, "");
  static_assert(std::is_same<Polymorphic<Foo, Bar>, Polymorphic<TypeListImpl<Foo, Bar>>>::value, "");
  static_assert(Polymorphic<Foo, Bar>::T_TYPELIST_SIZE == 2u, "");
  static_assert(std::is_same<Polymorphic<Foo, Bar>::T_TYPELIST, TypeListImpl<Foo, Bar>>::value, "");
}

TEST(TypeSystemTest, PolymorphicSmokeTestOneType) {
  using namespace struct_definition_test;

  {
    Polymorphic<Foo> p(make_unique<Foo>());
    const Polymorphic<Foo>& cp(p);

    {
      ASSERT_TRUE(p.Has<Foo>());
      const auto& foo = p.Value<Foo>();
      EXPECT_EQ(42u, foo.i);
    }
    {
      ASSERT_TRUE(cp.Has<Foo>());
      const auto& foo = cp.Value<Foo>();
      EXPECT_EQ(42u, foo.i);
    }

    ++p.Value<Foo>().i;

    EXPECT_EQ(43u, p.Value<Foo>().i);
    EXPECT_EQ(43u, cp.Value<Foo>().i);

    p = Foo(100u);
    EXPECT_EQ(100u, p.Value<Foo>().i);
    EXPECT_EQ(100u, cp.Value<Foo>().i);

    p = static_cast<const Foo&>(Foo(101u));
    EXPECT_EQ(101u, p.Value<Foo>().i);
    EXPECT_EQ(101u, cp.Value<Foo>().i);

    p = std::move(Foo(102u));
    EXPECT_EQ(102u, p.Value<Foo>().i);
    EXPECT_EQ(102u, cp.Value<Foo>().i);

    p = make_unique<Foo>(103u);
    EXPECT_EQ(103u, p.Value<Foo>().i);
    EXPECT_EQ(103u, cp.Value<Foo>().i);

    // TODO(dkorolev): Unsafe? Remove?
    p = new Foo(104u);
    EXPECT_EQ(104u, p.Value<Foo>().i);
    EXPECT_EQ(104u, cp.Value<Foo>().i);
  }

  {
    struct Visitor {
      std::string s;
      void operator()(const Foo& foo) { s += "Foo " + current::strings::ToString(foo.i) + '\n'; }
    };
    Visitor v;
    {
      Polymorphic<Foo> p(Foo(501u));
      p.Call(v);
      EXPECT_EQ("Foo 501\n", v.s);
    }
    {
      const Polymorphic<Foo> p(Foo(502u));
      p.Call(v);
      EXPECT_EQ("Foo 501\nFoo 502\n", v.s);
    }
  }

  {
    std::string s;
    const auto lambda =
        [&s](const Foo& foo) { s += "lambda: Foo " + current::strings::ToString(foo.i) + '\n'; };
    {
      Polymorphic<Foo> p(Foo(601u));
      p.Call(lambda);
      EXPECT_EQ("lambda: Foo 601\n", s);
    }
    {
      const Polymorphic<Foo> p(Foo(602u));
      p.Call(lambda);
      EXPECT_EQ("lambda: Foo 601\nlambda: Foo 602\n", s);
    }
  }

  {
    const Polymorphic<Foo> p((Foo()));
    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValue) {
    }
    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValueOfType<Bar>) {
    }
  }
}

TEST(TypeSystemTest, PolymorphicSmokeTestMultipleTypes) {
  using namespace struct_definition_test;

  struct Visitor {
    std::string s;
    void operator()(const Bar&) { s = "Bar"; }
    void operator()(const Foo& foo) { s = "Foo " + current::strings::ToString(foo.i); }
    void operator()(const DerivedFromFoo& object) {
      s = "DerivedFromFoo [" + current::strings::ToString(object.bar.v1.size()) + "]";
    }
  };
  Visitor v;

  {
    Polymorphic<Bar, Foo, DerivedFromFoo> p((Bar()));
    const Polymorphic<Bar, Foo, DerivedFromFoo>& cp = p;

    p.Call(v);
    EXPECT_EQ("Bar", v.s);
    cp.Call(v);
    EXPECT_EQ("Bar", v.s);

    p = Foo(1u);

    p.Call(v);
    EXPECT_EQ("Foo 1", v.s);
    cp.Call(v);
    EXPECT_EQ("Foo 1", v.s);

    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValue) {
    }
    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValueOfType<Bar>) {
    }

    p = make_unique<DerivedFromFoo>();
    p.Call(v);
    EXPECT_EQ("DerivedFromFoo [0]", v.s);
    cp.Call(v);
    EXPECT_EQ("DerivedFromFoo [0]", v.s);

    p.Value<DerivedFromFoo>().bar.v1.resize(3);
    p.Call(v);
    EXPECT_EQ("DerivedFromFoo [3]", v.s);
    cp.Call(v);
    EXPECT_EQ("DerivedFromFoo [3]", v.s);

    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValue) {
    }
    try {
      p.Value<Bar>();
      ASSERT_TRUE(false);
    } catch (NoValueOfType<Bar>) {
    }
  }
}

TEST(TypeSystemTest, PolymorphicRequiredAndOptional) {
  using namespace struct_definition_test;

  static_assert(Polymorphic<Foo, Bar>::IS_REQUIRED, "");
  static_assert(!Polymorphic<Foo, Bar>::IS_OPTIONAL, "");

  static_assert(!OptionalPolymorphic<Foo, Bar>::IS_REQUIRED, "");
  static_assert(OptionalPolymorphic<Foo, Bar>::IS_OPTIONAL, "");

  {
    Polymorphic<Foo, Bar> a = Foo(1);
    EXPECT_EQ(1ull, a.Value<Foo>().i);

    Polymorphic<Foo, Bar> b = std::move(a);
    EXPECT_EQ(1ull, b.Value<Foo>().i);
  }

  {
    OptionalPolymorphic<Foo, Bar> x;
    const bool b1 = x;
    EXPECT_FALSE(b1);
    x = Bar(2);
    const bool b2 = x;
    EXPECT_TRUE(b2);

    try {
      x.Value<Foo>();
      ASSERT_TRUE(false);
    } catch (NoValueOfType<Foo>) {
    }

    Polymorphic<Foo, Bar> y = std::move(x);
    static_cast<void>(y);
  }

  {
    OptionalPolymorphic<Foo, Bar> p;
    try {
      Polymorphic<Foo, Bar> q = std::move(p);
      ASSERT_TRUE(false);
    } catch (UninitializedRequiredPolymorphic) {
    }
    try {
      Polymorphic<Foo, Bar> r = std::move(p);
      ASSERT_TRUE(false);
    } catch (UninitializedRequiredPolymorphicOfType<Foo, Bar>) {
    }
  }
}

namespace struct_definition_test {
CURRENT_STRUCT(WithTimestampUS) {
  CURRENT_FIELD(t, std::chrono::microseconds);
  CURRENT_TIMESTAMP(t);
};
CURRENT_STRUCT(WithTimestampUInt64) {
  CURRENT_FIELD(another_t, int64_t);
  CURRENT_TIMESTAMP(another_t);
};
}  // namespace struct_definition_test

TEST(TypeSystemTest, TimestampSimple) {
  using namespace struct_definition_test;
  {
    WithTimestampUS a;
    a.t = std::chrono::microseconds(42ull);
    EXPECT_EQ(42ll, MicroTimestampOf(a).count());
    EXPECT_EQ(42ll, MicroTimestampOf(const_cast<const WithTimestampUS&>(a)).count());
  }
  {
    WithTimestampUInt64 x;
    x.another_t = 43ull;
    EXPECT_EQ(43ll, MicroTimestampOf(x).count());
    EXPECT_EQ(43ll, MicroTimestampOf(const_cast<const WithTimestampUInt64&>(x)).count());
  }
}

namespace struct_definition_test {
CURRENT_STRUCT(WithTimestampPolymorphic) {
  CURRENT_FIELD(magic, (Polymorphic<WithTimestampUS, WithTimestampUInt64>));
  CURRENT_CONSTRUCTOR(WithTimestampPolymorphic)(const WithTimestampUS& magic) : magic(magic) {}
  CURRENT_CONSTRUCTOR(WithTimestampPolymorphic)(const WithTimestampUInt64& magic) : magic(magic) {}
  CURRENT_TIMESTAMP(magic);
};
}  // namespace struct_definition_test

TEST(TypeSystemTest, TimestampPolymorphic) {
  using namespace struct_definition_test;

  WithTimestampUS a;
  a.t = std::chrono::microseconds(101);
  WithTimestampUInt64 b;
  b.another_t = 102ull;

  WithTimestampPolymorphic z1(a);
  EXPECT_EQ(101ll, MicroTimestampOf(z1).count());
  z1.magic.Value<WithTimestampUS>().t = std::chrono::microseconds(201);
  EXPECT_EQ(201ll, MicroTimestampOf(z1).count());

  WithTimestampPolymorphic z2(b);
  EXPECT_EQ(102ll, MicroTimestampOf(z2).count());
  z2.magic.Value<WithTimestampUInt64>().another_t = 202ull;
  EXPECT_EQ(202ll, MicroTimestampOf(z2).count());
}

namespace struct_definition_test {
CURRENT_STRUCT(DisabledDefaultConstructor) { CURRENT_FIELD(meh, (Polymorphic<Foo, Bar>)); };
}  // namespace struct_definition_test

TEST(TypeSystemTest, CanWorkAroundDisabledDefaultConstructor) {
  using namespace struct_definition_test;

  {
    using original = decltype(std::declval<Foo>().i);
    using modified = decltype(std::declval<Foo::DEFAULT_CONSTRUCTIBLE_TYPE>().i);
    static_assert(std::is_same<original, uint64_t>::value, "");
    static_assert(std::is_same<modified, uint64_t>::value, "");
  }

  {
    using original = decltype(std::declval<Baz>().v2);
    using modified = decltype(std::declval<Baz::DEFAULT_CONSTRUCTIBLE_TYPE>().v2);
    static_assert(std::is_same<original, std::vector<Foo>>::value, "");
    static_assert(std::is_same<modified, std::vector<Foo>>::value, "");
  }

  {
    using original = decltype(std::declval<DisabledDefaultConstructor>().meh);
    using modified = decltype(std::declval<DisabledDefaultConstructor::DEFAULT_CONSTRUCTIBLE_TYPE>().meh);
    static_assert(std::is_same<original, Polymorphic<Foo, Bar>>::value, "");
    static_assert(std::is_same<modified, OptionalPolymorphic<Foo, Bar>>::value, "");
  }
}
