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

#define CURRENT_MOCK_TIME

#include "../port.h"

#include "enum.h"
#include "struct.h"
#include "optional.h"
#include "variant.h"
#include "timestamp.h"

#include "../bricks/strings/strings.h"
#include "../bricks/dflags/dflags.h"
#include "../3rdparty/gtest/gtest-main-with-dflags.h"

#include "reflection/test.cc"
#include "serialization/test.cc"
#include "schema/test.cc"
#include "evolution/test.cc"

namespace struct_definition_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) {
  CURRENT_FIELD(i, uint64_t, 0u);
  CURRENT_CONSTRUCTOR(Foo)(uint64_t i = 42u) : i(i) {}
  uint64_t twice_i() const { return i * 2; }
  bool operator<(const Foo& rhs) const { return i < rhs.i; }
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
  CURRENT_FIELD_DESCRIPTION(v1, "We One.");
  CURRENT_FIELD_DESCRIPTION(v4, "We Four.");
};

CURRENT_STRUCT(DerivedFromFoo, Foo) {
  CURRENT_DEFAULT_CONSTRUCTOR(DerivedFromFoo) : SUPER(100u) {}
  CURRENT_CONSTRUCTOR(DerivedFromFoo)(size_t x) : SUPER(x * 1001u) {}
  CURRENT_FIELD(baz, Baz);
};

// clang-format off
CURRENT_STRUCT(DerivedFromDerivedFromFoo, DerivedFromFoo){
  CURRENT_DEFAULT_CONSTRUCTOR(DerivedFromDerivedFromFoo) : SUPER(1u) {}
  CURRENT_CONSTRUCTOR(DerivedFromDerivedFromFoo)(size_t x) : SUPER(x + 1) {}
};
// clang-format on

CURRENT_STRUCT(WithVector) {
  CURRENT_FIELD(v, std::vector<std::string>);
  size_t vector_size() const { return v.size(); }
};

CURRENT_STRUCT_T(Templated) {
  CURRENT_FIELD(i, uint32_t);
  CURRENT_FIELD(t, T);
  CURRENT_CONSTRUCTOR_T(Templated)(uint32_t i, const T& t) : i(i), t(t) {}
};

CURRENT_STRUCT_T(TemplatedDerivedFromFoo, Foo) {
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, T);
  CURRENT_CONSTRUCTOR_T(TemplatedDerivedFromFoo)(const uint32_t x, const std::string& s, const T& t)
      : SUPER(x * 1000001u), s(s), t(t) {}
};

CURRENT_STRUCT(NonTemplateDerivedFromTemplatedDerivedFromFooString, TemplatedDerivedFromFoo<std::string>){
  CURRENT_CONSTRUCTOR(NonTemplateDerivedFromTemplatedDerivedFromFooString)(const uint32_t x,
                                                                           const std::string& s,
                                                                           const std::string& t) : SUPER(x, s, t){}
};

#ifdef CURRENT_STRUCTS_SUPPORT_DERIVING_FROM_TEMPLATED_STRUCTS
CURRENT_STRUCT_T(TemplateDerivedFromTemplatedDerivedFromFooString, TemplatedDerivedFromFoo<T>){
  CURRENT_CONSTRUCTOR_T(TemplateDerivedFromTemplatedDerivedFromFooString)(const uint32_t x,
                                                                          const std::string& s,
                                                                          const T& t) : SUPER<T>(x, s, t){}
};
#endif  // CURRENT_STRUCTS_SUPPORT_DERIVING_FROM_TEMPLATED_STRUCTS

static_assert(IS_VALID_CURRENT_STRUCT(Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Baz), "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(DerivedFromFoo), "Struct `DerivedFromFoo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Templated<bool>), "Struct `Templated<>` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Templated<Foo>), "Struct `Templated<>` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(TemplatedDerivedFromFoo<bool>),
              "Struct `TemplatedDerivedFromFoo<>` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(TemplatedDerivedFromFoo<Foo>),
              "Struct `TemplatedDerivedFromFoo<>` was not properly declared.");

}  // namespace struct_definition_test

// Confirm that Current data types defined in a namespace are accessible from outside it.
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Baz), "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

namespace some_other_namespace {

// Confirm that Current data types defined in one namespace are accessible from another one.
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Baz), "Struct `Baz` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace some_other_namespace

namespace struct_definition_test {

// Properly declared structures.
CURRENT_STRUCT(Empty){};
CURRENT_STRUCT(EmptyDerived, Empty){};

static_assert(IS_VALID_CURRENT_STRUCT(Empty), "`Empty` must pass `IS_VALID_CURRENT_STRUCT` check.");
static_assert(IS_VALID_CURRENT_STRUCT(EmptyDerived), "`EmptyDerived` must pass `IS_VALID_CURRENT_STRUCT` check.");

// Improperly declared structures.
struct WrongStructNotCurrentStruct {
  int x;
};
struct WrongDerivedStructNotCurrentStruct : ::current::CurrentStruct {};
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

TEST(TypeSystemTest, FieldCounter) {
  using namespace struct_definition_test;
  {
    EXPECT_EQ(0u, current::reflection::FieldCounter<Empty>::value + 0u);
    EXPECT_EQ(0u, current::reflection::FieldCounter<EmptyDerived>::value + 0u);
    EXPECT_EQ(1u, current::reflection::FieldCounter<Foo>::value + 0u);
    EXPECT_EQ(1u, current::reflection::FieldCounter<Bar>::value + 0u);
    EXPECT_EQ(5u, current::reflection::FieldCounter<Baz>::value + 0u);
    EXPECT_EQ(1u, current::reflection::FieldCounter<DerivedFromFoo>::value + 0u);
    EXPECT_EQ(2u, current::reflection::FieldCounter<Templated<bool>>::value + 0u);
    EXPECT_EQ(2u, current::reflection::FieldCounter<Templated<Empty>>::value + 0u);
    EXPECT_EQ(2u, current::reflection::FieldCounter<TemplatedDerivedFromFoo<bool>>::value + 0u);
    EXPECT_EQ(2u, current::reflection::FieldCounter<TemplatedDerivedFromFoo<Empty>>::value + 0u);
  }
  {
    EXPECT_EQ(0u, current::reflection::TotalFieldCounter<Empty>::value + 0u);
    EXPECT_EQ(0u, current::reflection::TotalFieldCounter<EmptyDerived>::value + 0u);
    EXPECT_EQ(1u, current::reflection::TotalFieldCounter<Foo>::value + 0u);
    EXPECT_EQ(1u, current::reflection::TotalFieldCounter<Bar>::value + 0u);
    EXPECT_EQ(5u, current::reflection::TotalFieldCounter<Baz>::value + 0u);
    EXPECT_EQ(2u, current::reflection::TotalFieldCounter<DerivedFromFoo>::value + 0u);
    EXPECT_EQ(2u, current::reflection::TotalFieldCounter<Templated<bool>>::value + 0u);
    EXPECT_EQ(2u, current::reflection::TotalFieldCounter<Templated<Empty>>::value + 0u);
    EXPECT_EQ(3u, current::reflection::TotalFieldCounter<TemplatedDerivedFromFoo<bool>>::value + 0u);
    EXPECT_EQ(3u, current::reflection::TotalFieldCounter<TemplatedDerivedFromFoo<Empty>>::value + 0u);
  }
}

TEST(TypeSystemTest, FieldDescriptions) {
  using namespace struct_definition_test;
  EXPECT_STREQ("We One.", (current::reflection::FieldDescriptions::template Description<Baz, 0>()));
  EXPECT_STREQ(nullptr, (current::reflection::FieldDescriptions::template Description<Baz, 1>()));
  EXPECT_STREQ(nullptr, (current::reflection::FieldDescriptions::template Description<Baz, 2>()));
  EXPECT_STREQ("We Four.", (current::reflection::FieldDescriptions::template Description<Baz, 3>()));
  EXPECT_STREQ(nullptr, (current::reflection::FieldDescriptions::template Description<Baz, 4>()));
}

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

TEST(TypeSystemTest, ExistsForNonVariants) {
  using namespace struct_definition_test;

  Foo foo;
  EXPECT_TRUE(Exists<Foo>(foo));
  EXPECT_FALSE(Exists<Bar>(foo));
  EXPECT_FALSE(Exists<int>(foo));
  const Foo& foo_cref = foo;
  EXPECT_TRUE(Exists<Foo>(foo_cref));
  EXPECT_FALSE(Exists<Bar>(foo_cref));
  EXPECT_FALSE(Exists<int>(foo_cref));
  Foo& foo_ref = foo;
  EXPECT_TRUE(Exists<Foo>(foo_ref));
  EXPECT_FALSE(Exists<Bar>(foo_ref));
  Foo&& foo_rref = std::move(foo);
  EXPECT_TRUE(Exists<Foo>(foo_rref));
  EXPECT_FALSE(Exists<Bar>(foo_rref));
  EXPECT_FALSE(Exists<int>(foo_rref));

  EXPECT_TRUE(Exists<int>(42));
  EXPECT_FALSE(Exists<int>(foo));
  EXPECT_FALSE(Exists<int>(foo_cref));
  EXPECT_FALSE(Exists<int>(foo_ref));
  EXPECT_FALSE(Exists<int>(foo_rref));
}

TEST(TypeSystemTest, ValueOfMutableAndImmutableObjects) {
  using namespace struct_definition_test;

  int x;
  int& x_ref = x;
  const int& x_cref = x;
  const int cx = 1;
  static_assert(std::is_same<decltype(Value(x)), int&>::value, "");
  static_assert(std::is_same<decltype(Value(x_ref)), int&>::value, "");
  static_assert(std::is_same<decltype(Value(x_cref)), const int&>::value, "");
  static_assert(std::is_same<decltype(Value(std::move(x))), int&&>::value, "");
  static_assert(std::is_same<decltype(Value(cx)), const int&>::value, "");

  Foo foo(42);
  EXPECT_TRUE(Exists<Foo>(foo));
  EXPECT_EQ(42ull, Value(foo).i);
  EXPECT_EQ(42ull, Value(Value(foo)).i);

  uint64_t& i_ref = Value<Foo&>(foo).i;
  i_ref += 10ull;
  EXPECT_EQ(52ull, Value(foo).i);
  EXPECT_EQ(52ull, Value(Value(foo)).i);

  const Foo& foo_cref1 = foo;
  const Foo& foo_cref2 = Value<const Foo&>(foo_cref1);
  const Foo& foo_cref3 = Value<const Foo&>(foo_cref2);

  ASSERT_TRUE(&foo_cref1 == &foo);
  ASSERT_TRUE(&foo_cref2 == &foo);
  ASSERT_TRUE(&foo_cref3 == &foo);

  const uint64_t& i_cref = foo_cref3.i;
  ASSERT_TRUE(&i_ref == &i_cref);

  EXPECT_EQ(52ull, foo_cref1.i);
  EXPECT_EQ(52ull, foo_cref2.i);
  EXPECT_EQ(52ull, foo_cref3.i);
  EXPECT_EQ(52ull, i_cref);

  ++i_ref;

  EXPECT_EQ(53ull, foo_cref1.i);
  EXPECT_EQ(53ull, foo_cref2.i);
  EXPECT_EQ(53ull, foo_cref3.i);
  EXPECT_EQ(53ull, i_cref);
}

TEST(TypeSystemTest, CopyDoesItsJob) {
  using namespace struct_definition_test;

  Foo a(1u);
  Foo b(a);
  EXPECT_EQ(1u, a.i);
  EXPECT_EQ(1u, b.i);

  a.i = 3u;
  EXPECT_EQ(3u, a.i);
  EXPECT_EQ(1u, b.i);

  Foo c;
  c = a;
  EXPECT_EQ(3u, a.i);
  EXPECT_EQ(3u, c.i);

  a.i = 5u;
  EXPECT_EQ(5u, a.i);
  EXPECT_EQ(3u, c.i);
}

TEST(TypeSystemTest, ConstructingViaInitializerListIncludingSuper) {
  using namespace struct_definition_test;

  {
    DerivedFromFoo one;
    EXPECT_EQ(100u, one.i);
    DerivedFromFoo two(123u);
    EXPECT_EQ(123123u, two.i);
  }

  {
    DerivedFromDerivedFromFoo one;
    EXPECT_EQ(1001u, one.i);
    DerivedFromDerivedFromFoo two(123u);
    EXPECT_EQ(124124u, two.i);
  }

  {
    {
      TemplatedDerivedFromFoo<uint32_t> test(42u, "foo", 1u);
      EXPECT_EQ(42000042u, test.i);
      EXPECT_EQ("foo", test.s);
      EXPECT_EQ(1u, test.t);
    }
    {
      TemplatedDerivedFromFoo<std::string> test(42u, "foo", "test");
      EXPECT_EQ(42000042u, test.i);
      EXPECT_EQ("foo", test.s);
      EXPECT_EQ("test", test.t);
    }
  }

  {
    NonTemplateDerivedFromTemplatedDerivedFromFooString test(1u, "s", "t");
    EXPECT_EQ(1000001u, test.i);
    EXPECT_EQ("s", test.s);
    EXPECT_EQ("t", test.t);
  }

#ifdef CURRENT_STRUCTS_SUPPORT_DERIVING_FROM_TEMPLATED_STRUCTS
  {
    {
      TemplateDerivedFromTemplatedDerivedFromFooString<std::string> test(1u, "s", "t");
      EXPECT_EQ(1000001u, test.i);
      EXPECT_EQ("s", test.s);
      EXPECT_EQ("t", test.t);
    }
    {
      TemplateDerivedFromTemplatedDerivedFromFooString<bool> test(1u, "s", true);
      EXPECT_EQ(1000001u, test.i);
      EXPECT_EQ("s", test.s);
      EXPECT_TRUE(test.t);
    }
  }
#endif  // CURRENT_STRUCTS_SUPPORT_DERIVING_FROM_TEMPLATED_STRUCTS
};

TEST(TypeSystemTest, ConstructingTemplatedStructs) {
  using namespace struct_definition_test;

  {
    {
      Templated<uint32_t> test(1u, 42u);
      EXPECT_EQ(1u, test.i);
      EXPECT_EQ(42u, test.t);
    }
    {
      Templated<std::string> test(2u, "foo");
      EXPECT_EQ(2u, test.i);
      EXPECT_EQ("foo", test.t);
    }
  }
}

TEST(TypeSystemTest, ImmutableOptional) {
  using struct_definition_test::Foo;

  // POD version.
  {
    ImmutableOptional<int> foo(100);
    const ImmutableOptional<int>& foo_cref = foo;
    static_assert(std::is_same<decltype(Value(foo)), int>::value, "");
    static_assert(std::is_same<decltype(Value(foo_cref)), int>::value, "");
    const ImmutableOptional<int> bar(200);
    static_assert(std::is_same<decltype(Value(bar)), int>::value, "");
  }
  {
    ImmutableOptional<int> foo(100);
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(100, foo.ValueImpl());
    EXPECT_EQ(100, static_cast<int>(Value(foo)));
    EXPECT_EQ(100, static_cast<int>(Value(Value(foo))));
  }
  {
    ImmutableOptional<int> bar(nullptr);
    ASSERT_FALSE(Exists(bar));
    try {
      Value(bar);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
  }

  // Non-POD version.
  {
    ImmutableOptional<Foo> foo(100);
    const ImmutableOptional<Foo>& foo_cref = foo;
    static_assert(std::is_same<decltype(Value(foo)), const Foo&>::value, "");
    static_assert(std::is_same<decltype(Value(foo_cref)), const Foo&>::value, "");
    const ImmutableOptional<Foo> bar(200);
    static_assert(std::is_same<decltype(Value(bar)), const Foo&>::value, "");
  }
  {
    ImmutableOptional<Foo> meh(nullptr);
    ASSERT_FALSE(Exists(meh));
    try {
      Value(meh);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValueOfType<Foo>) {
    }
  }
  {
    Foo bare;
    ASSERT_TRUE(Exists(bare));
    EXPECT_EQ(42u, Value(bare).i);
    EXPECT_EQ(42u, Value(Value(bare)).i);
  }
  {
    Foo bare;
    ImmutableOptional<Foo> wrapped(FromBarePointer(), &bare);
    ASSERT_TRUE(Exists(wrapped));
    EXPECT_EQ(42u, wrapped.ValueImpl().i);
    EXPECT_EQ(42u, Value(wrapped).i);
    EXPECT_EQ(42u, Value(Value(wrapped)).i);
  }
  {
    Foo bare;
    ImmutableOptional<Foo> wrapped(FromBarePointer(), &bare);
    const ImmutableOptional<Foo>& double_wrapped(wrapped);
    ASSERT_TRUE(Exists(double_wrapped));
    EXPECT_EQ(42u, double_wrapped.ValueImpl().i);
    EXPECT_EQ(42u, Value(double_wrapped).i);
    EXPECT_EQ(42u, Value(Value(double_wrapped)).i);
  }
  {
    const auto lambda = [](int x) -> ImmutableOptional<int> { return x; };
    ASSERT_TRUE(Exists(lambda(101)));
    EXPECT_EQ(102, lambda(102).ValueImpl());
    EXPECT_EQ(102, Value(lambda(102)));
  }
}

TEST(TypeSystemTest, Optional) {
  using struct_definition_test::Foo;

  // POD version: `Value()` return type.
  {
    Optional<int> foo(200);
    const Optional<int>& foo_cref = foo;
    static_assert(std::is_same<decltype(Value(foo)), int&>::value, "");
    static_assert(std::is_same<decltype(Value(foo_cref)), int>::value, "");
    static_assert(std::is_same<decltype(Value(std::move(foo))), int&>::value, "");
    const Optional<int> bar(200);
    static_assert(std::is_same<decltype(Value(bar)), int>::value, "");
  }
  // POD version: Initialize in ctor.
  {
    Optional<int> foo(200);
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(200, foo.ValueImpl());
    EXPECT_EQ(200, Value(foo));
  }
  // POD version: Create empty, initialize by assignment.
  {
    Optional<int> foo;
    EXPECT_FALSE(Exists(foo));
    foo = 1;
    EXPECT_TRUE(Exists(foo));
    EXPECT_EQ(1, Value(foo));
  }
  // POD version: Exception.
  {
    Optional<int> foo(200);
    ASSERT_TRUE(Exists(foo));
    foo = nullptr;
    ASSERT_FALSE(Exists(foo));
    try {
      Value(foo);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
  }
  // POD version: Using `Value()`.
  {
    Optional<int> foo(100);
    const auto foo_value = Value(foo);
    const auto& foo_value_ref = Value(foo);
    EXPECT_EQ(100, Value(foo));
    EXPECT_EQ(100, foo_value);
    EXPECT_EQ(100, foo_value_ref);
    foo = 200;
    EXPECT_EQ(200, Value(foo));
    EXPECT_EQ(100, foo_value);
    EXPECT_EQ(200, foo_value_ref);
  }

  // Non-POD version: `Value()` return type.
  {
    Optional<Foo> foo(200);
    const Optional<Foo>& foo_cref = foo;
    static_assert(std::is_same<decltype(Value(foo)), Foo&>::value, "");
    static_assert(std::is_same<decltype(Value(foo_cref)), const Foo&>::value, "");
    const Optional<Foo> bar(200);
    static_assert(std::is_same<decltype(Value(bar)), const Foo&>::value, "");
  }
  // Non-POD version: Construct from `Foo&`.
  {
    Foo f(42u);
    Optional<Foo> foo(static_cast<Foo&>(f));
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(42u, Value(foo).i);
  }
  // Non-POD version: Construct from `const Foo&`.
  {
    Foo f(42u);
    Optional<Foo> foo(static_cast<const Foo&>(f));
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(42u, Value(foo).i);
  }
  // Non-POD version: Construct from `Foo&&`.
  {
    Optional<Foo> foo(Foo(42u));
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(42u, Value(foo).i);
  }
  // Non-POD version: Construct from `const Optional<Foo>&`.
  {
    Optional<Foo> foo(Foo(42));
    Optional<Foo> bar(static_cast<const Optional<Foo>&>(foo));
    ASSERT_TRUE(Exists(foo));
    Value(foo).i = 100u;
    ASSERT_TRUE(Exists(bar));
    EXPECT_EQ(100u, Value(foo).i);
    EXPECT_EQ(42u, Value(bar).i);
  }
  // Non-POD version: Construct from `Optional<Foo>&&`.
  {
    Optional<Foo> foo(Foo(42));
    ASSERT_TRUE(Exists(foo));
    EXPECT_EQ(42u, Value(foo).i);
    Optional<Foo> bar(std::move(foo));
    ASSERT_FALSE(Exists(foo));
    ASSERT_TRUE(Exists(bar));
    EXPECT_EQ(42u, Value(bar).i);
  }
  // Non-POD version: Create empty, initialize by rvalue assignment.
  {
    Optional<Foo> foo(Foo(100u));
    Optional<Foo> bar;
    EXPECT_TRUE(Exists(foo));
    EXPECT_FALSE(Exists(bar));
    bar = std::move(foo);
    ASSERT_FALSE(Exists(foo));
    ASSERT_TRUE(Exists(bar));
    EXPECT_EQ(100u, Value(bar).i);
  }
  // Non-POD version: Exception.
  {
    Optional<Foo> foo(Foo(1u));
    foo = nullptr;
    try {
      Value(foo);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
  }
}

namespace optionals_test {

template <template <typename...> class OPTIONAL_TYPE>
void TestComparisonOperatorsForPOD() {
  OPTIONAL_TYPE<int> i1(1), j1(1), k2(2);
  OPTIONAL_TYPE<int> empty(nullptr), empty2(nullptr);

  // Compare two `[Immutable]Optional<int>`-s.
  EXPECT_TRUE(i1 == i1);
  EXPECT_TRUE(i1 == j1);
  EXPECT_FALSE(i1 == k2);
  EXPECT_FALSE(i1 == empty);
  EXPECT_TRUE(empty == empty2);

  EXPECT_FALSE(i1 != i1);
  EXPECT_FALSE(i1 != j1);
  EXPECT_TRUE(i1 != k2);
  EXPECT_TRUE(i1 != empty);
  EXPECT_FALSE(empty != empty2);

  EXPECT_FALSE(i1 < i1);
  EXPECT_FALSE(i1 < j1);
  EXPECT_TRUE(i1 < k2);
  EXPECT_FALSE(i1 < empty);
  EXPECT_TRUE(empty < i1);
  EXPECT_FALSE(empty < empty2);

  EXPECT_TRUE(i1 <= i1);
  EXPECT_TRUE(i1 <= j1);
  EXPECT_TRUE(i1 <= k2);
  EXPECT_FALSE(i1 <= empty);
  EXPECT_TRUE(empty <= i1);
  EXPECT_TRUE(empty <= empty2);

  EXPECT_FALSE(i1 > i1);
  EXPECT_FALSE(i1 > j1);
  EXPECT_FALSE(i1 > k2);
  EXPECT_TRUE(i1 > empty);
  EXPECT_FALSE(empty > i1);
  EXPECT_FALSE(empty > empty2);

  EXPECT_TRUE(i1 >= i1);
  EXPECT_TRUE(i1 >= j1);
  EXPECT_FALSE(i1 >= k2);
  EXPECT_TRUE(i1 >= empty);
  EXPECT_FALSE(empty >= i1);
  EXPECT_TRUE(empty >= empty2);

  // Compare `[Immutable]Optional<int>` with `const int&`.
  EXPECT_TRUE(i1 == 1);
  EXPECT_TRUE(1 == i1);
  EXPECT_FALSE(i1 == 0);
  EXPECT_FALSE(0 == i1);
  EXPECT_FALSE(empty == 1);
  EXPECT_FALSE(1 == empty);

  EXPECT_FALSE(i1 != 1);
  EXPECT_FALSE(1 != i1);
  EXPECT_TRUE(i1 != 0);
  EXPECT_TRUE(0 != i1);
  EXPECT_TRUE(empty != 1);
  EXPECT_TRUE(1 != empty);

  EXPECT_FALSE(i1 < 1);
  EXPECT_FALSE(1 < i1);
  EXPECT_FALSE(i1 < 0);
  EXPECT_TRUE(0 < i1);
  EXPECT_TRUE(i1 < 2);
  EXPECT_FALSE(2 < i1);
  EXPECT_TRUE(empty < 1);
  EXPECT_FALSE(1 < empty);

  EXPECT_TRUE(i1 <= 1);
  EXPECT_TRUE(1 <= i1);
  EXPECT_FALSE(i1 <= 0);
  EXPECT_TRUE(0 <= i1);
  EXPECT_TRUE(i1 <= 2);
  EXPECT_FALSE(2 <= i1);
  EXPECT_TRUE(empty <= 1);
  EXPECT_FALSE(1 <= empty);

  EXPECT_FALSE(i1 > 1);
  EXPECT_FALSE(1 > i1);
  EXPECT_TRUE(i1 > 0);
  EXPECT_FALSE(0 > i1);
  EXPECT_FALSE(i1 > 2);
  EXPECT_TRUE(2 > i1);
  EXPECT_FALSE(empty > 1);
  EXPECT_TRUE(1 > empty);

  EXPECT_TRUE(i1 >= 1);
  EXPECT_TRUE(1 >= i1);
  EXPECT_TRUE(i1 >= 0);
  EXPECT_FALSE(0 >= i1);
  EXPECT_FALSE(i1 >= 2);
  EXPECT_TRUE(2 >= i1);
  EXPECT_FALSE(empty >= 1);
  EXPECT_TRUE(1 >= empty);
}

template <template <typename...> class OPTIONAL_TYPE>
void TestComparisonOperatorsForNonPOD() {
  OPTIONAL_TYPE<std::string> i1("1"), j1("1"), k2("2");
  OPTIONAL_TYPE<std::string> empty(nullptr), empty2(nullptr);
  const std::string const_str0("0"), const_str1("1"), const_str2("2");

  // Compare two `[Immutable]Optional<std::string>`-s.
  EXPECT_TRUE(i1 == i1);
  EXPECT_TRUE(i1 == j1);
  EXPECT_FALSE(i1 == k2);
  EXPECT_FALSE(i1 == empty);
  EXPECT_TRUE(empty == empty2);

  EXPECT_FALSE(i1 != i1);
  EXPECT_FALSE(i1 != j1);
  EXPECT_TRUE(i1 != k2);
  EXPECT_TRUE(i1 != empty);
  EXPECT_FALSE(empty != empty2);

  EXPECT_FALSE(i1 < i1);
  EXPECT_FALSE(i1 < j1);
  EXPECT_TRUE(i1 < k2);
  EXPECT_FALSE(i1 < empty);
  EXPECT_TRUE(empty < i1);
  EXPECT_FALSE(empty < empty2);

  EXPECT_TRUE(i1 <= i1);
  EXPECT_TRUE(i1 <= j1);
  EXPECT_TRUE(i1 <= k2);
  EXPECT_FALSE(i1 <= empty);
  EXPECT_TRUE(empty <= i1);
  EXPECT_TRUE(empty <= empty2);

  EXPECT_FALSE(i1 > i1);
  EXPECT_FALSE(i1 > j1);
  EXPECT_FALSE(i1 > k2);
  EXPECT_TRUE(i1 > empty);
  EXPECT_FALSE(empty > i1);
  EXPECT_FALSE(empty > empty2);

  EXPECT_TRUE(i1 >= i1);
  EXPECT_TRUE(i1 >= j1);
  EXPECT_FALSE(i1 >= k2);
  EXPECT_TRUE(i1 >= empty);
  EXPECT_FALSE(empty >= i1);
  EXPECT_TRUE(empty >= empty2);

  // Compare `[Immutable]Optional<std::string>` with `const std::string&`.
  EXPECT_TRUE(i1 == const_str1);
  EXPECT_TRUE(const_str1 == i1);
  EXPECT_FALSE(i1 == const_str0);
  EXPECT_FALSE(const_str0 == i1);
  EXPECT_FALSE(empty == const_str1);
  EXPECT_FALSE(const_str1 == empty);

  EXPECT_FALSE(i1 != const_str1);
  EXPECT_FALSE(const_str1 != i1);
  EXPECT_TRUE(i1 != const_str0);
  EXPECT_TRUE(const_str0 != i1);
  EXPECT_TRUE(empty != const_str1);
  EXPECT_TRUE(const_str1 != empty);

  EXPECT_FALSE(i1 < const_str1);
  EXPECT_FALSE(const_str1 < i1);
  EXPECT_FALSE(i1 < const_str0);
  EXPECT_TRUE(const_str0 < i1);
  EXPECT_TRUE(i1 < const_str2);
  EXPECT_FALSE(const_str2 < i1);
  EXPECT_TRUE(empty < const_str1);
  EXPECT_FALSE(const_str1 < empty);

  EXPECT_TRUE(i1 <= const_str1);
  EXPECT_TRUE(const_str1 <= i1);
  EXPECT_FALSE(i1 <= const_str0);
  EXPECT_TRUE(const_str0 <= i1);
  EXPECT_TRUE(i1 <= const_str2);
  EXPECT_FALSE(const_str2 <= i1);
  EXPECT_TRUE(empty <= const_str1);
  EXPECT_FALSE(const_str1 <= empty);

  EXPECT_FALSE(i1 > const_str1);
  EXPECT_FALSE(const_str1 > i1);
  EXPECT_TRUE(i1 > const_str0);
  EXPECT_FALSE(const_str0 > i1);
  EXPECT_FALSE(i1 > const_str2);
  EXPECT_TRUE(const_str2 > i1);
  EXPECT_FALSE(empty > const_str1);
  EXPECT_TRUE(const_str1 > empty);

  EXPECT_TRUE(i1 >= const_str1);
  EXPECT_TRUE(const_str1 >= i1);
  EXPECT_TRUE(i1 >= const_str0);
  EXPECT_FALSE(const_str0 >= i1);
  EXPECT_FALSE(i1 >= const_str2);
  EXPECT_TRUE(const_str2 >= i1);
  EXPECT_FALSE(empty >= const_str1);
  EXPECT_TRUE(const_str1 >= empty);
}

}  // namespace optionals_test

TEST(TypeSystemTest, OptionalComparisonOperators) {
  using namespace optionals_test;

  // Test `ImmutableOptional<>`.
  TestComparisonOperatorsForPOD<ImmutableOptional>();
  TestComparisonOperatorsForNonPOD<ImmutableOptional>();

  // Test `Optional<>`.
  TestComparisonOperatorsForPOD<Optional>();
  TestComparisonOperatorsForNonPOD<Optional>();
}

namespace enum_class_test {
CURRENT_ENUM(Fruits, uint32_t){APPLE = 1u, ORANGE = 2u};
}  // namespace enum_class_test

TEST(TypeSystemTest, EnumRegistration) {
  using current::reflection::EnumName;
  EXPECT_EQ("Fruits", EnumName<enum_class_test::Fruits>());
}

TEST(TypeSystemTest, VariantStaticAsserts) {
  using namespace struct_definition_test;

  static_assert(is_same_or_compile_error<Variant<Foo>, Variant<Foo>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo>, Variant<TypeList<Foo>>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo>, Variant<SlowTypeList<Foo, Foo>>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo>, Variant<TypeListImpl<Foo>>>::value, "");
  static_assert(Variant<Foo>::typelist_size == 1u, "");
  static_assert(is_same_or_compile_error<Variant<Foo>::typelist_t, TypeListImpl<Foo>>::value, "");

  static_assert(is_same_or_compile_error<Variant<Foo, Bar>, Variant<Foo, Bar>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo, Bar>, Variant<TypeList<Foo, Bar>>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo, Bar>, Variant<SlowTypeList<Foo, Bar, Foo>>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo, Bar>, Variant<SlowTypeList<Foo, Bar, TypeList<Bar>>>>::value, "");
  static_assert(is_same_or_compile_error<Variant<Foo, Bar>, Variant<TypeListImpl<Foo, Bar>>>::value, "");
  static_assert(Variant<Foo, Bar>::typelist_size == 2u, "");
  static_assert(is_same_or_compile_error<Variant<Foo, Bar>::typelist_t, TypeListImpl<Foo, Bar>>::value, "");
}

TEST(TypeSystemTest, EqualTypelistVariantsCopyAndMove) {
  using namespace struct_definition_test;

  // Copy empty.
  {
    Variant<Foo, Bar> empty_source;
    Variant<Foo, Bar> destination(empty_source);
    EXPECT_FALSE(destination.ExistsImpl());
    EXPECT_FALSE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_FALSE((Exists<Variant<Foo, Bar>>(destination)));
  }
  {
    Variant<Foo, Bar> empty_source;
    Variant<Foo, Bar> destination;
    destination = empty_source;
    EXPECT_FALSE(destination.ExistsImpl());
    EXPECT_FALSE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_FALSE((Exists<Variant<Foo, Bar>>(destination)));
  }

  // Move empty.
  {
    Variant<Foo, Bar> empty_source;
    Variant<Foo, Bar> destination(std::move(empty_source));
    EXPECT_FALSE(destination.ExistsImpl());
    EXPECT_FALSE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_FALSE((Exists<Variant<Foo, Bar>>(destination)));
  }
  {
    Variant<Foo, Bar> empty_source;
    Variant<Foo, Bar> destination;
    destination = std::move(empty_source);
    EXPECT_FALSE(destination.ExistsImpl());
    EXPECT_FALSE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_FALSE((Exists<Variant<Foo, Bar>>(destination)));
  }

  // Copy non-empty.
  {
    Variant<Foo, Bar> source(Foo(100u));
    Variant<Foo, Bar> destination(source);
    Value<Foo>(source).i = 101u;
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Foo>(destination).i);
    EXPECT_EQ(101u, Value<Foo>(source).i);
    EXPECT_TRUE((Exists<Variant<Foo, Bar>>(destination)));
    EXPECT_EQ(100u, (Value<Foo>(Value<Variant<Foo, Bar>>(destination)).i));
    EXPECT_EQ(101u, (Value<Foo>(Value<Variant<Foo, Bar>>(source)).i));
  }
  {
    Variant<Foo, Bar> source(Bar(100u));
    Variant<Foo, Bar> destination;
    destination = source;
    EXPECT_TRUE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_TRUE(Exists<Bar>(destination));
    Value<Bar>(source).j = 101u;
    EXPECT_EQ(100u, Value<Bar>(destination).j);
    EXPECT_EQ(101u, Value<Bar>(source).j);
    EXPECT_TRUE((Exists<Variant<Foo, Bar>>(destination)));
    EXPECT_EQ(100u, (Value<Bar>(Value<Variant<Foo, Bar>>(destination)).j));
    EXPECT_EQ(101u, (Value<Bar>(Value<Variant<Foo, Bar>>(source)).j));
  }

  // Move non-empty.
  {
    Variant<Foo, Bar> source(Foo(100u));
    EXPECT_TRUE(Exists(source));
    EXPECT_TRUE(Exists<Foo>(source));
    EXPECT_FALSE(Exists<Bar>(source));
    Variant<Foo, Bar> destination(std::move(source));
    EXPECT_FALSE(Exists(source));
    EXPECT_FALSE(Exists<Foo>(source));
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Foo>(destination).i);
    EXPECT_TRUE((Exists<Variant<Foo, Bar>>(destination)));
    EXPECT_EQ(100u, (Value<Foo>(Value<Variant<Foo, Bar>>(destination)).i));
  }
  {
    Variant<Foo, Bar> source(Foo(101u));
    Variant<Foo, Bar> destination;
    destination = std::move(source);
    EXPECT_FALSE(Exists(source));
    EXPECT_FALSE(Exists<Foo>(source));
    EXPECT_TRUE(Exists(destination));
    EXPECT_FALSE(Exists(source));
    EXPECT_TRUE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_EQ(101u, Value<Foo>(destination).i);
    EXPECT_TRUE((Exists<Variant<Foo, Bar>>(destination)));
    EXPECT_EQ(101u, (Value<Foo>(Value<Variant<Foo, Bar>>(destination)).i));
  }
}

TEST(TypeSystemTest, IntersectingTypelistVariantsCopyAndMove) {
  using namespace struct_definition_test;

  // Copy.
  {
    Variant<Foo, Bar> source(Foo(100u));
    Variant<Baz, Foo> destination(source);
    Value<Foo>(source).i = 101u;
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Foo>(destination).i);
    EXPECT_EQ(101u, Value<Foo>(source).i);
    EXPECT_TRUE((Exists<Variant<Baz, Foo>>(destination)));
    EXPECT_EQ(100u, (Value<Foo>(Value<Variant<Baz, Foo>>(destination)).i));
    EXPECT_EQ(101u, (Value<Foo>(Value<Variant<Foo, Bar>>(source)).i));
  }
  {
    Variant<Foo, Bar> source(Bar(100u));
    Variant<Bar> destination;
    destination = source;
    EXPECT_TRUE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_TRUE(Exists<Bar>(destination));
    Value<Bar>(source).j = 101u;
    EXPECT_EQ(100u, Value<Bar>(destination).j);
    EXPECT_EQ(101u, Value<Bar>(source).j);
    EXPECT_TRUE((Exists<Variant<Bar>>(destination)));
    EXPECT_EQ(100u, (Value<Bar>(Value<Variant<Bar>>(destination)).j));
    EXPECT_EQ(101u, (Value<Bar>(Value<Variant<Foo, Bar>>(source)).j));
  }
  {
    Variant<Bar> source(Bar(100u));
    Variant<Foo, Bar> destination;
    destination = source;
    EXPECT_TRUE(Exists(destination));
    EXPECT_FALSE(Exists<Foo>(destination));
    EXPECT_TRUE(Exists<Bar>(destination));
    Value<Bar>(source).j = 101u;
    EXPECT_EQ(100u, Value<Bar>(destination).j);
    EXPECT_EQ(101u, Value<Bar>(source).j);
    EXPECT_TRUE((Exists<Variant<Foo, Bar>>(destination)));
    EXPECT_EQ(100u, (Value<Bar>(Value<Variant<Foo, Bar>>(destination)).j));
    EXPECT_EQ(101u, (Value<Bar>(Value<Variant<Bar>>(source)).j));
  }
  {
    Variant<Bar> source(Bar(100u));
    Variant<Foo, Baz> destination;
    EXPECT_THROW(destination = source, IncompatibleVariantTypeException<Bar>);
  }

  // Move.
  {
    Variant<Foo, Bar> source(Foo(100u));
    Variant<Baz, Foo> destination(std::move(source));
    EXPECT_FALSE(Exists(source));
    EXPECT_FALSE(Exists<Foo>(source));
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Foo>(destination));
    EXPECT_FALSE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Foo>(destination).i);
  }
  {
    Variant<Foo, Bar> source(Bar(100u));
    Variant<Bar> destination;
    destination = std::move(source);
    EXPECT_FALSE(Exists(source));
    EXPECT_FALSE(Exists<Bar>(source));
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Bar>(destination).j);
  }
  {
    Variant<Bar> source(Bar(100u));
    Variant<Foo, Bar> destination;
    destination = std::move(source);
    EXPECT_FALSE(Exists(source));
    EXPECT_FALSE(Exists<Bar>(source));
    EXPECT_TRUE(Exists(destination));
    EXPECT_TRUE(Exists<Bar>(destination));
    EXPECT_EQ(100u, Value<Bar>(destination).j);
  }
  {
    Variant<Bar> source(Bar(100u));
    Variant<Foo, Baz> destination;
    EXPECT_THROW(destination = std::move(source), IncompatibleVariantTypeException<Bar>);
  }
}

TEST(TypeSystemTest, VariantSmokeTestOneType) {
  using namespace struct_definition_test;
  using current::BypassVariantTypeCheck;

  {
    Variant<Foo> p(BypassVariantTypeCheck(), std::make_unique<Foo>());
    const Variant<Foo>& cp(p);

    {
      ASSERT_TRUE(p.VariantExistsImpl<Foo>());
      const auto& foo = p.VariantValueImpl<Foo>();
      EXPECT_EQ(42u, foo.i);
    }
    {
      ASSERT_TRUE(cp.VariantExistsImpl<Foo>());
      const auto& foo = cp.VariantValueImpl<Foo>();
      EXPECT_EQ(42u, foo.i);
    }
    {
      ASSERT_TRUE(p.VariantExistsImpl<Foo>());
      const auto& foo = Value<Foo>(p);
      EXPECT_EQ(42u, foo.i);
    }
    {
      ASSERT_TRUE(cp.VariantExistsImpl<Foo>());
      const auto& foo = Value<Foo>(cp);
      EXPECT_EQ(42u, foo.i);
    }

    ++p.VariantValueImpl<Foo>().i;

    EXPECT_EQ(43u, p.VariantValueImpl<Foo>().i);
    EXPECT_EQ(43u, cp.VariantValueImpl<Foo>().i);

    p = Foo(100u);
    EXPECT_EQ(100u, p.VariantValueImpl<Foo>().i);
    EXPECT_EQ(100u, cp.VariantValueImpl<Foo>().i);

    p = static_cast<const Foo&>(Foo(101u));
    EXPECT_EQ(101u, p.VariantValueImpl<Foo>().i);
    EXPECT_EQ(101u, cp.VariantValueImpl<Foo>().i);

    p = Foo(102u);
    EXPECT_EQ(102u, p.VariantValueImpl<Foo>().i);
    EXPECT_EQ(102u, cp.VariantValueImpl<Foo>().i);

    p.UncheckedMoveFromUniquePtr(std::make_unique<Foo>(103u));
    EXPECT_EQ(103u, p.VariantValueImpl<Foo>().i);
    EXPECT_EQ(103u, cp.VariantValueImpl<Foo>().i);
  }

  {
    struct Visitor {
      std::string s;
      void operator()(const Foo& foo) { s += "Foo " + current::ToString(foo.i) + '\n'; }
    };
    Visitor v;
    {
      Variant<Foo> p(Foo(501u));
      p.Call(v);
      EXPECT_EQ("Foo 501\n", v.s);
    }
    {
      const Variant<Foo> p(Foo(502u));
      p.Call(v);
      EXPECT_EQ("Foo 501\nFoo 502\n", v.s);
    }
  }

  {
    std::string s;
    const auto lambda = [&s](const Foo& foo) { s += "lambda: Foo " + current::ToString(foo.i) + '\n'; };
    {
      Variant<Foo> p(Foo(601u));
      p.Call(lambda);
      EXPECT_EQ("lambda: Foo 601\n", s);
    }
    {
      const Variant<Foo> p(Foo(602u));
      p.Call(lambda);
      EXPECT_EQ("lambda: Foo 601\nlambda: Foo 602\n", s);
    }
  }

  {
    const Variant<Foo> p((Foo()));
    try {
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
    try {
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValueOfType<Bar>) {
    }
    try {
      Value<Bar>(p);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
    try {
      Value<Bar>(p);
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValueOfType<Bar>) {
    }
  }
}

TEST(TypeSystemTest, VariantSmokeTestMultipleTypes) {
  using namespace struct_definition_test;

  struct Visitor {
    std::string s;
    void operator()(const Bar&) { s = "Bar"; }
    void operator()(const Foo& foo) { s = "Foo " + current::ToString(foo.i); }
    void operator()(const DerivedFromFoo& object) {
      s = "DerivedFromFoo [" + current::ToString(object.baz.v1.size()) + "]";
    }
  };
  Visitor v;

  {
    Variant<Bar, Foo, DerivedFromFoo> p((Bar()));
    const Variant<Bar, Foo, DerivedFromFoo>& cp = p;

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
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
    try {
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValueOfType<Bar>) {
    }

    p = DerivedFromFoo();
    p.Call(v);
    EXPECT_EQ("DerivedFromFoo [0]", v.s);
    cp.Call(v);
    EXPECT_EQ("DerivedFromFoo [0]", v.s);

    p.VariantValueImpl<DerivedFromFoo>().baz.v1.resize(3);
    p.Call(v);
    EXPECT_EQ("DerivedFromFoo [3]", v.s);
    cp.Call(v);
    EXPECT_EQ("DerivedFromFoo [3]", v.s);

    try {
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValue) {
    }
    try {
      p.VariantValueImpl<Bar>();
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (NoValueOfType<Bar>) {
    }
  }
}

namespace struct_definition_test {
CURRENT_STRUCT(WithTimestampUS) {
  CURRENT_FIELD(t, std::chrono::microseconds);
  CURRENT_USE_FIELD_AS_TIMESTAMP(t);
};
CURRENT_STRUCT(WithTimestampUInt64) {
  CURRENT_FIELD(another_t, int64_t);
  CURRENT_USE_FIELD_AS_TIMESTAMP(another_t);
};
}  // namespace struct_definition_test

TEST(TypeSystemTest, TimestampSimple) {
  using namespace struct_definition_test;
  using current::MicroTimestampOf;
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
CURRENT_STRUCT(WithTimestampVariant) {
  CURRENT_FIELD(magic, (Variant<WithTimestampUS, WithTimestampUInt64>));
  CURRENT_CONSTRUCTOR(WithTimestampVariant)(const WithTimestampUS& magic) : magic(magic) {}
  CURRENT_CONSTRUCTOR(WithTimestampVariant)(const WithTimestampUInt64& magic) : magic(magic) {}
  CURRENT_USE_FIELD_AS_TIMESTAMP(magic);
};
}  // namespace struct_definition_test

TEST(TypeSystemTest, TimestampVariant) {
  using namespace struct_definition_test;
  using current::MicroTimestampOf;

  WithTimestampUS a;
  a.t = std::chrono::microseconds(101);
  WithTimestampUInt64 b;
  b.another_t = 102ull;

  WithTimestampVariant z1(a);
  EXPECT_EQ(101ll, MicroTimestampOf(z1).count());
  z1.magic.VariantValueImpl<WithTimestampUS>().t = std::chrono::microseconds(201);
  EXPECT_EQ(201ll, MicroTimestampOf(z1).count());

  WithTimestampVariant z2(b);
  EXPECT_EQ(102ll, MicroTimestampOf(z2).count());
  z2.magic.VariantValueImpl<WithTimestampUInt64>().another_t = 202ull;
  EXPECT_EQ(202ll, MicroTimestampOf(z2).count());
}

TEST(TypeSystemTest, ConstructorsAndMemberFunctions) {
  using namespace struct_definition_test;
  {
    Foo foo(42u);
    EXPECT_EQ(84u, foo.twice_i());
  }
  {
    WithVector v;
    EXPECT_EQ(0u, v.vector_size());
    v.v.push_back("foo");
    v.v.push_back("bar");
    EXPECT_EQ(2u, v.vector_size());
  }
}

namespace struct_definition_test {

CURRENT_STRUCT(WithVariant) {
  CURRENT_FIELD(p, (Variant<Foo, Bar>));
  CURRENT_CONSTRUCTOR(WithVariant)(Foo foo) : p(foo) {}
};

}  // namespace struct_definition_test

TEST(TypeSystemTest, ComplexCloneCases) {
  using namespace struct_definition_test;

  {
    Variant<Foo, Bar> x(Foo(1));
    Variant<Foo, Bar> y = x;
    EXPECT_EQ(1u, Value<Foo>(y).i);
  }

  {
    // Confirm a copy is being made.
    WithVariant x(Foo(2));
    WithVariant y(Foo(0));
    EXPECT_EQ(2u, Value<Foo>(x.p).i);
    EXPECT_EQ(0u, Value<Foo>(y.p).i);
    y = x;
    x = Foo(3);
    EXPECT_EQ(3u, Value<Foo>(x.p).i);
    EXPECT_EQ(2u, Value<Foo>(y.p).i);
  }
}

#ifndef VARIANT_CHECKS_AT_RUNTIME_INSTEAD_OF_COMPILE_TIME
// Nested variants are only allowed if variant type checking is compile-time, not runtime.
TEST(TypeSystemTest, NestedVariants)
#else
TEST(TypeSystemTest, DISABLED_NestedVariants)
#endif  // VARIANT_CHECKS_AT_RUNTIME_INSTEAD_OF_COMPILE_TIME
{
  using namespace struct_definition_test;

  // The Variant in question is `Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>`.
  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v;
    ASSERT_FALSE((Exists<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)));
    ASSERT_FALSE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_FALSE(Exists<Empty>(v));
  }

  // Part 1: Test `Foo` from the first `Variant<Foo, Bar>`.
  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(Variant<Foo, Bar>(Foo(1)));
    ASSERT_TRUE((Exists<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)));
    ASSERT_TRUE((&Value<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)) == &v);
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Foo>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(1u, Value<Foo>(Value<Variant<Foo, Bar>>(v)).i);
    ASSERT_FALSE(Exists<Empty>(v));
  }

  {
    Variant<Foo, Bar> foo = Foo(1);
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(foo);
    ASSERT_TRUE((Exists<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)));
    ASSERT_TRUE((&Value<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)) == &v);
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Foo>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(1u, Value<Foo>(Value<Variant<Foo, Bar>>(v)).i);
    ASSERT_FALSE(Exists<Empty>(v));
  }

  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v;
    ASSERT_FALSE((Exists<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)));
    Variant<Foo, Bar> foo = Foo(1);
    v = foo;
    ASSERT_TRUE((Exists<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)));
    ASSERT_TRUE((&Value<Variant<Variant<Foo, Bar>, Variant<Bar, Baz>>>(v)) == &v);
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Foo>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(1u, Value<Foo>(Value<Variant<Foo, Bar>>(v)).i);
    ASSERT_FALSE(Exists<Empty>(v));
  }

  // Part 2: Test `Bar` from the first `Variant<Foo, Bar>`.
  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(Variant<Foo, Bar>(Bar(2u)));
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(2u, Value<Bar>(Value<Variant<Foo, Bar>>(v)).j);
  }

  {
    Variant<Foo, Bar> bar = Bar(2u);
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(bar);
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(2u, Value<Bar>(Value<Variant<Foo, Bar>>(v)).j);
  }

  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v;
    Variant<Foo, Bar> bar = Bar(2u);
    v = bar;
    ASSERT_TRUE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_FALSE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Foo, Bar>>(v))));
    EXPECT_EQ(2u, Value<Bar>(Value<Variant<Foo, Bar>>(v)).j);
  }

  // Part 3: Test `Bar` from the second `Variant<Bar, Baz>`.
  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(Variant<Bar, Baz>(Bar(3u)));
    ASSERT_FALSE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_TRUE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Bar, Baz>>(v))));
    EXPECT_EQ(3u, Value<Bar>(Value<Variant<Bar, Baz>>(v)).j);
  }

  {
    Variant<Bar, Baz> bar = Bar(3u);
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v(bar);
    ASSERT_FALSE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_TRUE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Bar, Baz>>(v))));
    EXPECT_EQ(3u, Value<Bar>(Value<Variant<Bar, Baz>>(v)).j);
  }

  {
    Variant<Variant<Foo, Bar>, Variant<Bar, Baz>> v;
    Variant<Bar, Baz> bar = Bar(3u);
    v = bar;
    ASSERT_FALSE((Exists<Variant<Foo, Bar>>(v)));
    ASSERT_TRUE((Exists<Variant<Bar, Baz>>(v)));
    ASSERT_TRUE((Exists<Bar>(Value<Variant<Bar, Baz>>(v))));
    EXPECT_EQ(3u, Value<Bar>(Value<Variant<Bar, Baz>>(v)).j);
  }
}
