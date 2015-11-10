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

#include <cstdint>

#include "reflection.h"
#include "schema.h"

#include "../../Bricks/strings/strings.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace reflection_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) { CURRENT_FIELD(i, uint64_t, 42u); };
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(v1, std::vector<uint64_t>);
  CURRENT_FIELD(v2, std::vector<Foo>);
  CURRENT_FIELD(v3, std::vector<std::vector<Foo>>);
  using map_string_string = std::map<std::string, std::string>;  // Sigh. -- D.K.
  CURRENT_FIELD(v4, map_string_string);
};
CURRENT_STRUCT(DerivedFromFoo, Foo) { CURRENT_FIELD(bar, Bar); };

using current::reflection::Reflector;

}  // namespace reflection_test

TEST(Reflection, DescribeCppStruct) {
  using namespace reflection_test;

  EXPECT_EQ(
      "struct Foo {\n"
      "  uint64_t i;\n"
      "};\n",
      Reflector().DescribeCppStruct<Foo>());

  EXPECT_EQ(
      "struct Bar {\n"
      "  std::vector<uint64_t> v1;\n"
      "  std::vector<Foo> v2;\n"
      "  std::vector<std::vector<Foo>> v3;\n"
      "  std::map<std::string,std::string> v4;\n"
      "};\n",
      Reflector().DescribeCppStruct<Bar>());

  EXPECT_EQ(
      "struct DerivedFromFoo : Foo {\n"
      "  Bar bar;\n"
      "};\n",
      Reflector().DescribeCppStruct<DerivedFromFoo>());

  EXPECT_EQ(9u, Reflector().KnownTypesCountForUnitTest());
}

TEST(Reflection, TypeID) {
  using namespace reflection_test;
  using current::reflection::ReflectedType_Struct;

  // TODO(dkorolev): Migrate to `Polymorphic<>` and avoid `dynamic_cast<>` here.
  const ReflectedType_Struct& bar = dynamic_cast<const ReflectedType_Struct&>(*Reflector().ReflectType<Bar>());
  EXPECT_EQ(9310000000000000028ull, static_cast<uint64_t>(bar.fields[0].first->type_id));
  EXPECT_EQ(9317693294631279482ull, static_cast<uint64_t>(bar.fields[1].first->type_id));
  EXPECT_EQ(9318642515553007349ull, static_cast<uint64_t>(bar.fields[2].first->type_id));
}

TEST(Reflection, CurrentStructInternals) {
  using namespace reflection_test;
  using namespace current::reflection;

  static_assert(std::is_same<SuperType<Foo>, CurrentSuper>::value, "");
  EXPECT_EQ(1u, FieldCounter<Foo>::value);

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

  static_assert(std::is_same<SuperType<Bar>, CurrentSuper>::value, "");
  EXPECT_EQ(4u, FieldCounter<Bar>::value);
  static_assert(std::is_same<SuperType<DerivedFromFoo>, Foo>::value, "");
  EXPECT_EQ(1u, FieldCounter<DerivedFromFoo>::value);
}

namespace reflection_test {

// TODO: move these asserts into sources?
static_assert(sizeof(float) == 4u, "Only 32-bit `float` is supported.");
static_assert(sizeof(double) == 8u, "Only 64-bit `double` is supported.");

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
  CURRENT_FIELD(flt, float, 1e38);
  CURRENT_FIELD(dbl, double, 1e308);
  // Other primitive types.
  CURRENT_FIELD(s, std::string, "The String");

  // Complex types.
  using pair_string_double = std::pair<std::string, double>;
  CURRENT_FIELD(pair_strdbl, pair_string_double);
  CURRENT_FIELD(vector_int32, std::vector<int32_t>);
  using map_string_string = std::map<std::string, std::string>;
  CURRENT_FIELD(map_strstr, map_string_string);
};
}

namespace reflection_test {

struct CollectFieldValues {
  std::vector<std::string>& output_;

  template <typename T>
  void operator()(const std::string&, const T& value) const {
    output_.push_back(bricks::strings::ToString(value));
  }

  template <typename T>
  void operator()(const std::string&, const std::vector<T>& value) const {
    output_.push_back('[' + bricks::strings::Join(value, ',') + ']');
  }

  template <typename TF, typename TS>
  void operator()(const std::string&, const std::pair<TF, TS>& value) const {
    output_.push_back(bricks::strings::ToString(value.first) + ':' + bricks::strings::ToString(value.second));
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
};

}  // namespace reflection_test

TEST(Reflection, VisitAllFields) {
  using namespace reflection_test;

  StructWithAllSupportedTypes all;
  all.pair_strdbl = {"Minus eight point five", -9.5};
  all.vector_int32 = {-1, -2, -4};
  all.map_strstr = {{"key1", "value1"}, {"key2", "value2"}};

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
      "Minus eight point five:-9.500000,"
      "[-1,-2,-4],"
      "[key1:value1,key2:value2]",
      bricks::strings::Join(result, ','));
}

namespace reflection_test {

CURRENT_STRUCT(X) { CURRENT_FIELD(i, int32_t); };
CURRENT_STRUCT(Y) { CURRENT_FIELD(v, std::vector<X>); };
CURRENT_STRUCT(Z, Y) {
  CURRENT_FIELD(d, double);
  CURRENT_FIELD(v2, std::vector<std::vector<Y>>);
};
}

TEST(Reflection, FullTypeSchema) {
  using namespace reflection_test;
  using current::reflection::FullTypeSchema;

  FullTypeSchema schema;
  schema.AddStruct<Z>();
  EXPECT_EQ(3u, schema.ordered_struct_list.size());
  EXPECT_EQ(3u, schema.struct_schemas.size());
  const uint64_t x_type_id = schema.ordered_struct_list[0];
  EXPECT_EQ("X", schema.struct_schemas[x_type_id].name);
  EXPECT_EQ(9000000000000000023ull, schema.struct_schemas[x_type_id].fields[0].first);
  EXPECT_EQ("i", schema.struct_schemas[x_type_id].fields[0].second);
  const uint64_t y_type_id = schema.ordered_struct_list[1];
  EXPECT_EQ("Y", schema.struct_schemas[y_type_id].name);
  EXPECT_EQ(9317693294612922990ull, schema.struct_schemas[y_type_id].fields[0].first);
  EXPECT_EQ("v", schema.struct_schemas[y_type_id].fields[0].second);
  const uint64_t z_type_id = schema.ordered_struct_list[2];
  EXPECT_EQ("Z", schema.struct_schemas[z_type_id].name);
  EXPECT_EQ(9000000000000000032ull, schema.struct_schemas[z_type_id].fields[0].first);
  EXPECT_EQ("d", schema.struct_schemas[z_type_id].fields[0].second);

  EXPECT_EQ("std::vector<X>", schema.CppDescription(schema.struct_schemas[y_type_id].fields[0].first));
  EXPECT_EQ("std::vector<std::vector<Y>>",
            schema.CppDescription(schema.struct_schemas[z_type_id].fields[1].first));
  EXPECT_EQ(
      "struct Z : Y {\n"
      "  double d;\n"
      "  std::vector<std::vector<Y>> v2;\n"
      "};\n",
      schema.CppDescription(z_type_id));
}
