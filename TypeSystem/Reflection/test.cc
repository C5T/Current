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

// This `test.cc` file is `#include`-d from `../test.cc`, and thus needs a header guard.

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_TEST_CC
#define CURRENT_TYPE_SYSTEM_REFLECTION_TEST_CC

#include <cstdint>

#include "reflection.h"

#include "../../Bricks/strings/strings.h"
#include "../../3rdparty/gtest/gtest-main.h"

namespace reflection_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) { CURRENT_FIELD(i, uint64_t, 42u); };
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(v1, std::vector<uint64_t>);
  CURRENT_FIELD(v2, std::vector<Foo>);
  CURRENT_FIELD(v3, std::vector<std::vector<Foo>>);
  CURRENT_FIELD(v4, (std::map<std::string, std::string>));
  CURRENT_FIELD_DESCRIPTION(v1, "This is v1.");
  CURRENT_FIELD_DESCRIPTION(v4, "This is v4.");
};

CURRENT_STRUCT(Baz) {
  CURRENT_FIELD(one, std::string);
  CURRENT_FIELD(two, std::string);
  CURRENT_FIELD(three, std::string);
  CURRENT_FIELD(blah, bool);
};

CURRENT_VARIANT(FooBarBaz, Foo, Bar, Baz);
CURRENT_VARIANT(AnotherFooBarBaz, Foo, Bar, Baz);  // To confirm TypeID-s are different for different names.

CURRENT_STRUCT(SimpleDerivedFromFoo, Foo) { CURRENT_FIELD(s, std::string); };
CURRENT_STRUCT(DerivedFromFoo, Foo) { CURRENT_FIELD(bar, Bar); };

CURRENT_STRUCT(SelfContainingA) { CURRENT_FIELD(v, std::vector<SelfContainingA>); };
CURRENT_STRUCT(SelfContainingB) { CURRENT_FIELD(v, std::vector<SelfContainingB>); };
CURRENT_STRUCT(SelfContainingC, SelfContainingA) {
  CURRENT_FIELD(v, std::vector<SelfContainingB>);
#ifndef CURRENT_WINDOWS
  CURRENT_FIELD(m, (std::map<std::string, SelfContainingC>));
#else
  typedef std::map<std::string, SelfContainingC> t_map_string_c;
  CURRENT_FIELD(m, t_map_string_c);
#endif
};

CURRENT_STRUCT_T(Templated) {
  CURRENT_FIELD(base, std::string);
  CURRENT_FIELD(extension, T);
};

using current::reflection::Reflector;

}  // namespace reflection_test

TEST(Reflection, StructAndVariant) {
  using namespace reflection_test;
  using current::reflection::ReflectedType_Struct;
  using current::reflection::ReflectedType_Variant;

  const ReflectedType_Struct& bar = Value<ReflectedType_Struct>(Reflector().ReflectType<Bar>());
  EXPECT_EQ(4u, bar.fields.size());
  EXPECT_EQ(9319767778871345347ull, static_cast<uint64_t>(bar.fields[0].type_id));
  EXPECT_EQ(9319865771553050731ull, static_cast<uint64_t>(bar.fields[1].type_id));
  EXPECT_EQ(9311949877586199388ull, static_cast<uint64_t>(bar.fields[2].type_id));
  EXPECT_EQ(9349351407460177576ull, static_cast<uint64_t>(bar.fields[3].type_id));
  EXPECT_EQ("v1", bar.fields[0].name);
  EXPECT_EQ("v2", bar.fields[1].name);
  EXPECT_EQ("v3", bar.fields[2].name);
  EXPECT_EQ("v4", bar.fields[3].name);
  EXPECT_TRUE(Exists(bar.fields[0].description));
  EXPECT_EQ("This is v1.", Value(bar.fields[0].description));
  EXPECT_FALSE(Exists(bar.fields[1].description));
  EXPECT_FALSE(Exists(bar.fields[2].description));
  EXPECT_TRUE(Exists(bar.fields[3].description));
  EXPECT_EQ("This is v4.", Value(bar.fields[3].description));
  EXPECT_EQ(bar.type_id, Value<ReflectedType_Struct>(Reflector().ReflectedTypeByTypeID(bar.type_id)).type_id);

  const ReflectedType_Struct& self_a = Value<ReflectedType_Struct>(Reflector().ReflectType<SelfContainingA>());
  EXPECT_EQ(1u, self_a.fields.size());
  EXPECT_EQ(9206664846159389537ull, static_cast<uint64_t>(self_a.type_id));
  EXPECT_EQ(9318143269698080259ull, static_cast<uint64_t>(self_a.fields[0].type_id));
  const ReflectedType_Struct& self_b = Value<ReflectedType_Struct>(Reflector().ReflectType<SelfContainingB>());
  EXPECT_EQ(1u, self_b.fields.size());
  EXPECT_EQ(9205249121542238939ull, static_cast<uint64_t>(self_b.type_id));
  EXPECT_EQ(9318143287813964755ull, static_cast<uint64_t>(self_b.fields[0].type_id));
  const ReflectedType_Struct& self_c = Value<ReflectedType_Struct>(Reflector().ReflectType<SelfContainingC>());
  EXPECT_EQ(2u, self_c.fields.size());
  EXPECT_EQ(9200251873128019120ull, static_cast<uint64_t>(self_c.type_id));
  EXPECT_EQ(9318143287813964755ull, static_cast<uint64_t>(self_c.fields[0].type_id));
  EXPECT_EQ(9345111461746810545ull, static_cast<uint64_t>(self_c.fields[1].type_id));

  EXPECT_NE(static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<FooBarBaz>()).type_id),
            static_cast<uint64_t>(
                Value<ReflectedType_Variant>(Reflector().ReflectType<Variant<Foo, Bar, Baz>>()).type_id));

  EXPECT_NE(
      static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<FooBarBaz>()).type_id),
      static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<AnotherFooBarBaz>()).type_id));

  {
    const auto& foo = Value<ReflectedType_Struct>(Reflector().ReflectType<Foo>());
    EXPECT_EQ(9203762249085213197ull, static_cast<uint64_t>(foo.type_id));

    const auto& bar = Value<ReflectedType_Struct>(Reflector().ReflectType<Bar>());
    EXPECT_EQ(9206387322237345681ull, static_cast<uint64_t>(bar.type_id));

    {
      const auto& templated_foo = Value<ReflectedType_Struct>(Reflector().ReflectType<Templated<Foo>>());
      EXPECT_EQ(2u, templated_foo.fields.size());
      EXPECT_EQ(9204032299541411163ull, static_cast<uint64_t>(templated_foo.type_id));
      EXPECT_EQ(9000000000000000042ull, static_cast<uint64_t>(templated_foo.fields[0].type_id));
      EXPECT_EQ(static_cast<uint64_t>(foo.type_id), static_cast<uint64_t>(templated_foo.fields[1].type_id));
    }

    {
      const auto& templated_bar = Value<ReflectedType_Struct>(Reflector().ReflectType<Templated<Bar>>());
      EXPECT_EQ(2u, templated_bar.fields.size());
      EXPECT_EQ(9202814639044342656ull, static_cast<uint64_t>(templated_bar.type_id));
      EXPECT_EQ(9000000000000000042ull, static_cast<uint64_t>(templated_bar.fields[0].type_id));
      EXPECT_EQ(static_cast<uint64_t>(bar.type_id), static_cast<uint64_t>(templated_bar.fields[1].type_id));
    }
  }
}

namespace reflection_test {
CURRENT_STRUCT(A){};
CURRENT_STRUCT(X){};
CURRENT_STRUCT(Y){};
namespace explicitly_declared_named_variant_one {
CURRENT_VARIANT(Variant_B_A_X_Y_E, A, X, Y);
}  // namespace reflection_test::explicitly_declared_named_variant_one
namespace explicitly_declared_named_variant_two {
CURRENT_VARIANT(Variant_B_A_X_Y_E, A, X, Y);
}  // namespace reflection_test::explicitly_declared_named_variant_two

static_assert(!std::is_same<explicitly_declared_named_variant_one::Variant_B_A_X_Y_E, Variant<A, X, Y>>::value,
              "");
static_assert(!std::is_same<explicitly_declared_named_variant_two::Variant_B_A_X_Y_E, Variant<A, X, Y>>::value,
              "");
static_assert(!std::is_same<explicitly_declared_named_variant_one::Variant_B_A_X_Y_E,
                            explicitly_declared_named_variant_two::Variant_B_A_X_Y_E>::value,
              "");

}  // namespace reflection_test

TEST(Reflection, VariantAndCurrentVariantHaveSameTypeID) {
  using one_t = reflection_test::explicitly_declared_named_variant_one::Variant_B_A_X_Y_E;
  using two_t = reflection_test::explicitly_declared_named_variant_two::Variant_B_A_X_Y_E;
  using vanilla_t = Variant<reflection_test::A, reflection_test::X, reflection_test::Y>;
  using current::reflection::Reflector;
  using current::reflection::ReflectedType_Variant;
  EXPECT_EQ(static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<one_t>()).type_id),
            static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<vanilla_t>()).type_id));
  EXPECT_EQ(static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<two_t>()).type_id),
            static_cast<uint64_t>(Value<ReflectedType_Variant>(Reflector().ReflectType<vanilla_t>()).type_id));
}

TEST(Reflection, CurrentStructInternals) {
  using namespace reflection_test;
  using namespace current::reflection;

  static_assert(std::is_same<SuperType<Foo>, ::current::CurrentStruct>::value, "");
  EXPECT_EQ(1u, FieldCounter<Foo>::value + 0u);

  Foo::CURRENT_REFLECTION([](TypeSelector<uint64_t>, const std::string& name) { EXPECT_EQ("i", name); },
                          Index<FieldTypeAndName, 0>());

  Foo foo;
  foo.i = 100u;
  foo.CURRENT_REFLECTION([](const std::string& name, const uint64_t& value) {
    EXPECT_EQ("i", name);
    EXPECT_EQ(100u, value);
  }, Index<FieldNameAndImmutableValue, 0>());

  foo.CURRENT_REFLECTION([](const std::string& name, uint64_t& value) {
    EXPECT_EQ("i", name);
    value = 123u;
  }, Index<FieldNameAndMutableValue, 0>());
  EXPECT_EQ(123u, foo.i);

  static_assert(std::is_same<SuperType<Bar>, ::current::CurrentStruct>::value, "");
  EXPECT_EQ(4u, FieldCounter<Bar>::value + 0u);
  static_assert(std::is_same<SuperType<DerivedFromFoo>, Foo>::value, "");
  EXPECT_EQ(1u, FieldCounter<DerivedFromFoo>::value + 0u);

  static_assert(std::is_same<TemplateInnerType<Bar>, void>::value, "");
  static_assert(std::is_same<TemplateInnerType<Templated<Bar>>, Bar>::value, "");
}

namespace reflection_test {

CURRENT_ENUM(Enum, uint32_t){Value1 = 1u, Value2 = 2u};

CURRENT_STRUCT(StructWithAllSupportedTypes) {
  // Integral.
  CURRENT_FIELD(b, bool, true);
  CURRENT_FIELD(c, char, 'Q');
  CURRENT_FIELD(uint8, uint8_t, UINT8_MAX);
  CURRENT_FIELD(uint16, uint16_t, UINT16_MAX);
  CURRENT_FIELD(uint32, uint32_t, UINT32_MAX);
  CURRENT_FIELD(uint64, uint64_t, UINT64_MAX);
  CURRENT_FIELD(int8, int8_t, INT8_MIN);
  CURRENT_FIELD(int16, int16_t, INT16_MIN);
  CURRENT_FIELD(int32, int32_t, INT32_MIN);
  CURRENT_FIELD(int64, int64_t, INT64_MIN);
  // Floating point.
  CURRENT_FIELD(flt, float, 1e38f);
  CURRENT_FIELD(dbl, double, 1e308);
  // Other primitive types.
  CURRENT_FIELD(s, std::string, "The String");
  CURRENT_FIELD(e, Enum, Enum::Value2);
#ifndef CURRENT_WINDOWS
  // STL containers.
  CURRENT_FIELD(pair_strdbl, (std::pair<std::string, double>));
  CURRENT_FIELD(vector_int32, std::vector<int32_t>);
  CURRENT_FIELD(map_strstr, (std::map<std::string, std::string>));
#else
  // STL containers.
  typedef std::map<std::string, std::string> t_pair_string_string;
  typedef std::pair<std::string, double> t_pair_string_double;
  CURRENT_FIELD(pair_strdbl, t_pair_string_double);
  CURRENT_FIELD(vector_int32, std::vector<int32_t>);
  CURRENT_FIELD(map_strstr, t_pair_string_string);
#endif
  // Current complex types.
  CURRENT_FIELD(optional_i, Optional<int32_t>);
  CURRENT_FIELD(optional_b, Optional<bool>);
};

struct CollectFieldValues {
  std::vector<std::string>& output_;

  template <typename T>
  std::enable_if_t<!std::is_enum<T>::value> operator()(const std::string&, const T& value) const {
    output_.push_back(current::ToString(value));
  }

  template <typename T>
  std::enable_if_t<std::is_enum<T>::value> operator()(const std::string&, const T& value) const {
    output_.push_back(current::ToString(static_cast<typename std::underlying_type<T>::type>(value)));
  }

  template <typename T>
  void operator()(const std::string&, const std::vector<T>& value) const {
    output_.push_back('[' + current::strings::Join(value, ',') + ']');
  }

  template <typename TF, typename TS>
  void operator()(const std::string&, const std::pair<TF, TS>& value) const {
    output_.push_back(current::ToString(value.first) + ':' + current::ToString(value.second));
  }

  template <typename TK, typename TV>
  void operator()(const std::string&, const std::map<TK, TV>& value) const {
    std::ostringstream oss;
    oss << '[';
    bool first = true;
    for (const auto& cit : value) {
      if (first) {
        first = false;
      } else {
        oss << ',';
      }
      oss << cit.first << ':' << cit.second;
    }
    oss << ']';
    output_.push_back(oss.str());
  }

  // Output `bool` using boolalpha.
  void operator()(const std::string&, bool value) const {
    std::ostringstream oss;
    oss << std::boolalpha << value;
    output_.push_back(oss.str());
  }

  // Output floating types in scientific notation.
  void operator()(const std::string&, float value) const {
    std::ostringstream oss;
    oss << value;
    output_.push_back(oss.str());
  }

  void operator()(const std::string&, double value) const {
    std::ostringstream oss;
    oss << value;
    output_.push_back(oss.str());
  }

  // Output `Optional`.
  template <typename T>
  void operator()(const std::string&, const Optional<T>& value) const {
    std::ostringstream oss;
    if (Exists(value)) {
      oss << Value(value);
    } else {
      oss << "null";
    }
    output_.push_back(oss.str());
  }
};

}  // namespace reflection_test

TEST(Reflection, VisitAllFieldsWithoutObject) {
  using namespace reflection_test;

  struct Collector {
    std::vector<std::string> names_and_types;
    void operator()(current::reflection::TypeSelector<std::string>, const std::string& name) {
      names_and_types.push_back("std::string " + name);
    }
    void operator()(current::reflection::TypeSelector<bool>, const std::string& name) {
      names_and_types.push_back("bool " + name);
    }
  };

  Collector collector;
  current::reflection::VisitAllFields<Baz, current::reflection::FieldTypeAndName>::WithoutObject(collector);

  EXPECT_EQ("std::string one,std::string two,std::string three,bool blah",
            current::strings::Join(collector.names_and_types, ','));
}

TEST(Reflection, VisitAllFieldsWithoutObjectAndGrabMemberPointers) {
  using namespace reflection_test;

  struct Pointers {
    std::map<std::string, std::string Baz::*> fields;
    void operator()(const std::string& name, std::string Baz::*ptr) { fields[name] = ptr; }
    void operator()(const std::string&, bool Baz::*) {}
  };

  Pointers ptrs;
  current::reflection::VisitAllFields<Baz, current::reflection::FieldNameAndPtr<Baz>>::WithoutObject(ptrs);

  Baz baz;
  EXPECT_EQ("", baz.one);
  baz.one = "FAIL";
  baz.*ptrs.fields["one"] = "PASS";
  EXPECT_EQ("PASS", baz.one);
}

TEST(Reflection, VisitAllFieldsWithObject) {
  using namespace reflection_test;

  StructWithAllSupportedTypes all;
  all.pair_strdbl = {"Minus eight point five", -9.5};
  all.vector_int32 = {-1, -2, -4};
  all.map_strstr = {{"key1", "value1"}, {"key2", "value2"}};
  all.optional_i = 128;  // Leaving `optional_b` empty.

  std::vector<std::string> result;
  CollectFieldValues values{result};
  current::reflection::VisitAllFields<StructWithAllSupportedTypes,
                                      current::reflection::FieldNameAndImmutableValue>::WithObject(all, values);
  EXPECT_EQ(
      "true,"
      "Q,"
      "255,65535,4294967295,18446744073709551615,"
      "-128,-32768,-2147483648,-9223372036854775808,"
      "1e+38,1e+308,"
      "The String,"
      "2,"
      "Minus eight point five:-9.500000,"
      "[-1,-2,-4],"
      "[key1:value1,key2:value2],"
      "128,null",
      current::strings::Join(result, ','));
}

TEST(Reflection, VisitAllFieldsForBaseType) {
  using namespace reflection_test;
  using namespace current::reflection;  // `current::reflection::{VisitAllFields,FieldNameAndImmutableValue}`.

  SimpleDerivedFromFoo derived;
  derived.i = 2016u;  // `SimpleDerivedFromFoo::Foo::i`.
  derived.s = "s";    // `SimpleDerivedFromFoo::s`.

  const Foo& base = derived;

  {
    // Visiting derived as derived returns `s`.
    std::vector<std::string> result;
    CollectFieldValues values{result};
    VisitAllFields<SimpleDerivedFromFoo, FieldNameAndImmutableValue>::WithObject(derived, values);
    EXPECT_EQ("s", current::strings::Join(result, ','));
  }

  {
    // Visiting derived as base returns `i`.
    std::vector<std::string> result;
    CollectFieldValues values{result};
    VisitAllFields<Foo, FieldNameAndImmutableValue>::WithObject(derived, values);
    EXPECT_EQ("2016", current::strings::Join(result, ','));
  }

  {
    // Visiting base as base returns `i`.
    std::vector<std::string> result;
    CollectFieldValues values{result};
    VisitAllFields<Foo, FieldNameAndImmutableValue>::WithObject(base, values);
    EXPECT_EQ("2016", current::strings::Join(result, ','));
  }
}

namespace reflection_test {

// Two identical yet different base structs.
CURRENT_STRUCT(BaseTypeOne){};
CURRENT_STRUCT(BaseTypeTwo){};

namespace one {
CURRENT_STRUCT(IdenticalCurrentStructWithDifferentBaseType, BaseTypeOne){};
}  // namespace reflection_test::one

namespace two {
CURRENT_STRUCT(IdenticalCurrentStructWithDifferentBaseType, BaseTypeTwo){};
}  // namespace reflection_test::two

using current::reflection::Reflector;

}  // namespace reflection_test

TEST(Reflection, BaseTypeMatters) {
  using namespace reflection_test;
  using current::reflection::ReflectedTypeBase;
  EXPECT_NE(static_cast<uint64_t>(Value<ReflectedTypeBase>(Reflector().ReflectType<BaseTypeOne>()).type_id),
            static_cast<uint64_t>(Value<ReflectedTypeBase>(Reflector().ReflectType<BaseTypeTwo>()).type_id));
  EXPECT_NE(static_cast<uint64_t>(
                Value<ReflectedTypeBase>(
                    Reflector().ReflectType<one::IdenticalCurrentStructWithDifferentBaseType>()).type_id),
            static_cast<uint64_t>(
                Value<ReflectedTypeBase>(
                    Reflector().ReflectType<two::IdenticalCurrentStructWithDifferentBaseType>()).type_id));
  EXPECT_EQ(9200000000962478099ull,
            static_cast<uint64_t>(Value<ReflectedTypeBase>(Reflector().ReflectType<BaseTypeOne>()).type_id));
  EXPECT_EQ(9200000001392004228ull,
            static_cast<uint64_t>(Value<ReflectedTypeBase>(Reflector().ReflectType<BaseTypeTwo>()).type_id));
  EXPECT_EQ(9205123477974540226ull,
            static_cast<uint64_t>(
                Value<ReflectedTypeBase>(
                    Reflector().ReflectType<one::IdenticalCurrentStructWithDifferentBaseType>()).type_id));
  EXPECT_EQ(9205123533591385154ull,
            static_cast<uint64_t>(
                Value<ReflectedTypeBase>(
                    Reflector().ReflectType<two::IdenticalCurrentStructWithDifferentBaseType>()).type_id));
}

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_TEST_CC
