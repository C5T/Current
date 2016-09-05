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

// This `test.cc` file is `#include`-d from `../test.cc`, and thus needs a header guard.

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TEST_CC
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TEST_CC

#include "binary.h"
#include "json.h"

#include "../struct.h"

#include "../Reflection/reflection.h"
#include "../Schema/schema.h"

#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace serialization_test {

CURRENT_ENUM(Enum, uint32_t){DEFAULT = 0u, SET = 100u};

CURRENT_STRUCT(Empty){};
CURRENT_STRUCT(AlternativeEmpty){};

CURRENT_STRUCT(Serializable) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(b, bool);
  CURRENT_FIELD(e, Enum);

  // Note: The user has to define default constructor when defining custom ones.
  CURRENT_DEFAULT_CONSTRUCTOR(Serializable) {}
  CURRENT_CONSTRUCTOR(Serializable)(int i, const std::string& s, bool b, Enum e) : i(i), s(s), b(b), e(e) {}
  CURRENT_CONSTRUCTOR(Serializable)(int i) : i(i), s(""), b(false), e(Enum::DEFAULT) {}

  bool operator<(const Serializable& rhs) const { return i < rhs.i; }
};

CURRENT_STRUCT(ComplexSerializable) {
  CURRENT_FIELD(j, uint64_t);
  CURRENT_FIELD(q, std::string);
  CURRENT_FIELD(v, std::vector<std::string>);
  CURRENT_FIELD(z, Serializable);

  CURRENT_DEFAULT_CONSTRUCTOR(ComplexSerializable) {}
  CURRENT_CONSTRUCTOR(ComplexSerializable)(char a, char b) {
    for (char c = a; c <= b; ++c) {
      v.push_back(std::string(1, c));
    }
  }
};

using simple_variant_t = Variant<Empty, AlternativeEmpty, Serializable, ComplexSerializable>;

CURRENT_STRUCT(ContainsVariant) { CURRENT_FIELD(variant, simple_variant_t); };

CURRENT_STRUCT(DerivedSerializable, Serializable) { CURRENT_FIELD(d, double); };

CURRENT_STRUCT(WithVectorOfPairs) { CURRENT_FIELD(v, (std::vector<std::pair<int32_t, std::string>>)); };

CURRENT_STRUCT(WithTrivialMap) { CURRENT_FIELD(m, (std::map<std::string, std::string>)); };

CURRENT_STRUCT(WithNontrivialMap) { CURRENT_FIELD(q, (std::map<Serializable, std::string>)); };

CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(i, Optional<int>);
  CURRENT_FIELD(b, Optional<bool>);
};

CURRENT_STRUCT(WithTime) {
  CURRENT_FIELD(number, uint64_t, 0ull);
  CURRENT_FIELD(micros, std::chrono::microseconds, std::chrono::microseconds(0));
};

namespace named_variant {

CURRENT_STRUCT(X) { CURRENT_FIELD(x, int32_t, 1); };

CURRENT_STRUCT(Y) { CURRENT_FIELD(y, int32_t, 2); };

CURRENT_STRUCT(Z) { CURRENT_FIELD(z, int32_t, 3); };

CURRENT_STRUCT(T) { CURRENT_FIELD(t, int32_t, 4); };

CURRENT_VARIANT(InnerA, X, Y);
CURRENT_VARIANT(InnerB, Z, T);

CURRENT_STRUCT(OuterA) { CURRENT_FIELD(a, InnerA); };
CURRENT_STRUCT(OuterB) { CURRENT_FIELD(b, InnerB); };

CURRENT_VARIANT(Q, OuterA, OuterB);

CURRENT_VARIANT(InnerVariant, WithVectorOfPairs, WithOptional);
CURRENT_STRUCT(WithInnerVariant) { CURRENT_FIELD(v, InnerVariant); };

}  // namespace serialization_test::named_variant
}  // namespace serialization_test

#if 0
// TODO(dkorolev): DIMA FIXME binary format.
TEST(Serialization, Binary) {
  using namespace serialization_test;

  const std::string tmp_file = current::FileSystem::GenTmpFileName();
  const auto tmp_file_remover = current::FileSystem::ScopedRmFile(tmp_file);
  {
    std::ofstream ofs(tmp_file);
    Serializable simple_object;
    simple_object.i = 42;
    simple_object.s = "foo";
    simple_object.b = true;
    simple_object.e = Enum::SET;
    SaveIntoBinary(ofs, simple_object);

    ComplexSerializable complex_object;
    complex_object.j = 43;
    complex_object.q = "bar";
    complex_object.v.push_back("one");
    complex_object.v.push_back("two");
    complex_object.z = Clone(simple_object);
    SaveIntoBinary(ofs, complex_object);

    DerivedSerializable derived_object;
    derived_object.i = 48;
    derived_object.s = "baz\0baz";
    derived_object.b = true;
    derived_object.e = Enum::SET;
    derived_object.d = 0.125;
    SaveIntoBinary(ofs, derived_object);

    WithNontrivialMap with_nontrivial_map;
    auto tmp = Clone(simple_object);
    with_nontrivial_map.q[std::move(tmp)] = "wow";
    with_nontrivial_map.q[Serializable(1, "one", false, Enum::DEFAULT)] = "yes";
    SaveIntoBinary(ofs, with_nontrivial_map);
  }
  {
    std::ifstream ifs(tmp_file);
    const Serializable a = LoadFromBinary<Serializable>(ifs);
    EXPECT_EQ(42ull, a.i);
    EXPECT_EQ("foo", a.s);
    EXPECT_TRUE(a.b);
    EXPECT_EQ(Enum::SET, a.e);

    const ComplexSerializable b = LoadFromBinary<ComplexSerializable>(ifs);
    EXPECT_EQ(43ull, b.j);
    EXPECT_EQ("bar", b.q);
    ASSERT_EQ(2u, b.v.size());
    EXPECT_EQ("one", b.v[0]);
    EXPECT_EQ("two", b.v[1]);
    EXPECT_EQ(42ull, b.z.i);
    EXPECT_EQ("foo", b.z.s);
    EXPECT_TRUE(b.z.b);
    EXPECT_EQ(Enum::SET, b.z.e);

    const DerivedSerializable c = LoadFromBinary<DerivedSerializable>(ifs);
    EXPECT_EQ(48ull, c.i);
    EXPECT_EQ("baz\0baz", c.s);
    EXPECT_TRUE(c.b);
    EXPECT_EQ(Enum::SET, c.e);
    EXPECT_DOUBLE_EQ(0.125, c.d);

    const WithNontrivialMap m = LoadFromBinary<WithNontrivialMap>(ifs);
    EXPECT_EQ(2u, m.q.size());
    EXPECT_EQ("yes", m.q.at(Serializable(1, "one", false, Enum::DEFAULT)));

    std::istringstream is("Invalid");
    ASSERT_THROW(LoadFromBinary<ComplexSerializable>(is), BinaryLoadFromStreamException);
  }
}
#endif

TEST(Serialization, JSON) {
  using namespace serialization_test;

  // Simple serialization.
  Serializable simple_object;
  simple_object.i = 0;
  simple_object.s = "";
  simple_object.b = false;
  simple_object.e = Enum::DEFAULT;

  EXPECT_EQ("{\"i\":0,\"s\":\"\",\"b\":false,\"e\":0}", JSON(simple_object));

  simple_object.i = 42;
  simple_object.s = "foo";
  simple_object.b = true;
  simple_object.e = Enum::SET;
  const std::string simple_object_as_json = JSON(simple_object);
  EXPECT_EQ("{\"i\":42,\"s\":\"foo\",\"b\":true,\"e\":100}", simple_object_as_json);

  {
    const Serializable a = ParseJSON<Serializable>(simple_object_as_json);
    EXPECT_EQ(42ull, a.i);
    EXPECT_EQ("foo", a.s);
    EXPECT_TRUE(a.b);
    EXPECT_EQ(Enum::SET, a.e);
  }

  // Nested serialization.
  ComplexSerializable complex_object;
  complex_object.j = 43;
  complex_object.q = "bar";
  complex_object.v.push_back("one");
  complex_object.v.push_back("two");
  complex_object.z = Clone(simple_object);

  const std::string complex_object_as_json = JSON(complex_object);
  EXPECT_EQ(
      "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":42,\"s\":\"foo\",\"b\":true,\"e\":100}}",
      complex_object_as_json);

  {
    const ComplexSerializable b = ParseJSON<ComplexSerializable>(complex_object_as_json);
    EXPECT_EQ(43ull, b.j);
    EXPECT_EQ("bar", b.q);
    ASSERT_EQ(2u, b.v.size());
    EXPECT_EQ("one", b.v[0]);
    EXPECT_EQ("two", b.v[1]);
    EXPECT_EQ(42ull, b.z.i);
    EXPECT_EQ("foo", b.z.s);
    EXPECT_TRUE(b.z.b);
    EXPECT_EQ(Enum::SET, b.z.e);

    ASSERT_THROW(ParseJSON<ComplexSerializable>("not a json"), InvalidJSONException);
  }

  // Complex serialization makes a copy.
  simple_object.i = 1000;
  EXPECT_EQ(42ull, complex_object.z.i);
  EXPECT_EQ(
      "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":42,\"s\":\"foo\",\"b\":true,\"e\":100}}",
      JSON(complex_object));

  // Derived struct serialization.
  DerivedSerializable derived_object;
  derived_object.i = 48;
  derived_object.s = "baz";
  derived_object.b = true;
  derived_object.e = Enum::SET;
  derived_object.d = 0.125;
  const std::string derived_object_as_json = JSON(derived_object);
  EXPECT_EQ("{\"i\":48,\"s\":\"baz\",\"b\":true,\"e\":100,\"d\":0.125}", derived_object_as_json);

  {
    const DerivedSerializable c = ParseJSON<DerivedSerializable>(derived_object_as_json);
    EXPECT_EQ(48ull, c.i);
    EXPECT_EQ("baz", c.s);
    EXPECT_TRUE(c.b);
    EXPECT_EQ(Enum::SET, c.e);
    EXPECT_DOUBLE_EQ(0.125, c.d);
  }

  // Serialization/deserialization of `std::vector<std::pair<...>>`.
  {
    WithVectorOfPairs with_vector_of_pairs;
    with_vector_of_pairs.v.emplace_back(-1, "foo");
    with_vector_of_pairs.v.emplace_back(1, "bar");
    EXPECT_EQ("{\"v\":[[-1,\"foo\"],[1,\"bar\"]]}", JSON(with_vector_of_pairs));
  }
  {
    const auto parsed = ParseJSON<WithVectorOfPairs>("{\"v\":[[-1,\"foo\"],[-2,\"bar\"],[100,\"baz\"]]}");
    ASSERT_EQ(3u, parsed.v.size());
    EXPECT_EQ(-1, parsed.v[0].first);
    EXPECT_EQ("foo", parsed.v[0].second);
    EXPECT_EQ(-2, parsed.v[1].first);
    EXPECT_EQ("bar", parsed.v[1].second);
    EXPECT_EQ(100, parsed.v[2].first);
    EXPECT_EQ("baz", parsed.v[2].second);
  }

  // Serializing an `std::map<>` with simple key type, which becomes a JSON object.
  {
    WithTrivialMap with_map;
    EXPECT_EQ("{\"m\":{}}", JSON(with_map));
    with_map.m["foo"] = "fizz";
    with_map.m["bar"] = "buzz";
    EXPECT_EQ("{\"m\":{\"bar\":\"buzz\",\"foo\":\"fizz\"}}", JSON(with_map));
  }
  {
    const auto parsed = ParseJSON<WithTrivialMap>("{\"m\":{}}");
    ASSERT_TRUE(parsed.m.empty());
  }
  {
    try {
      ParseJSON<WithTrivialMap>("{\"m\":[]}");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected map as object for `m`, got: []"), e.what());
    }
  }
  {
    const auto parsed = ParseJSON<WithTrivialMap>("{\"m\":{\"spock\":\"LLandP\",\"jedi\":\"MTFBWY\"}}");
    ASSERT_EQ(2u, parsed.m.size());
    EXPECT_EQ("LLandP", parsed.m.at("spock"));
    EXPECT_EQ("MTFBWY", parsed.m.at("jedi"));
  }
  // Serializing an `std::map<>` with complex key type, which becomes a JSON array of arrays.
  {
    WithNontrivialMap with_nontrivial_map;
    EXPECT_EQ("{\"q\":[]}", JSON(with_nontrivial_map));
    with_nontrivial_map.q[Clone(simple_object)] = "wow";
    EXPECT_EQ("{\"q\":[[{\"i\":1000,\"s\":\"foo\",\"b\":true,\"e\":100},\"wow\"]]}", JSON(with_nontrivial_map));
    with_nontrivial_map.q[Serializable(1, "one", false, Enum::DEFAULT)] = "yes";
    EXPECT_EQ(
        "{\"q\":[[{\"i\":1,\"s\":\"one\",\"b\":false,\"e\":0},\"yes\"],[{\"i\":1000,\"s\":\"foo\",\"b\":true,"
        "\"e\":100},\"wow\"]]"
        "}",
        JSON(with_nontrivial_map));
  }
  {
    const auto parsed = ParseJSON<WithNontrivialMap>("{\"q\":[]}");
    ASSERT_TRUE(parsed.q.empty());
  }
  {
    try {
      ParseJSON<WithNontrivialMap>("{\"q\":{}}");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected map as array for `q`, got: {}"), e.what());
    }
  }
  {
    // FIXME(dkorolev): Forgetting the `bool` in the JSON below results in bad exception messages.
    const auto parsed = ParseJSON<WithNontrivialMap>(
        "{\"q\":[[{\"i\":3,\"s\":\"three\",\"b\":true,\"e\":100},\"prime\"],[{\"i\":4,\"s\":\"four\",\"b\":"
        "false,\"e\":0},"
        "\"composite\"]]}");
    ASSERT_EQ(2u, parsed.q.size());
    EXPECT_EQ("prime", parsed.q.at(Serializable(3, "", true, Enum::SET)));
    EXPECT_EQ("composite", parsed.q.at(Serializable(4, "", false, Enum::DEFAULT)));
  }
}

TEST(Serialization, JSONExceptions) {
  using namespace serialization_test;

  // Invalid JSONs.
  ASSERT_THROW(ParseJSON<Serializable>("not a json"), InvalidJSONException);
  ASSERT_THROW(ParseJSON<ComplexSerializable>("not a json"), InvalidJSONException);

  ASSERT_THROW(ParseJSON<Serializable>(""), InvalidJSONException);
  ASSERT_THROW(ParseJSON<ComplexSerializable>(""), InvalidJSONException);

  // Valid JSONs with missing fields, or with fields of wrong types.
  try {
    ParseJSON<Serializable>("{}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `i`, got: missing field."), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":\"boo\"}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `i`, got: \"boo\""), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":[]}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `i`, got: []"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":{}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `i`, got: {}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":100}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: missing field."), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":42}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: 42"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":[]}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: []"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":{}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: {}"), e.what());
  }

  // Names of inner, nested, fields.
  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":\"error\",\"s\":\"foo\"}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `z.i`, got: \"error\""), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":null,\"s\":\"foo\"}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `z.i`, got: null"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"s\":\"foo\"}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected unsigned integer for `z.i`, got: missing field."), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",true],\"z\":{\"i\":0,\"s\":0}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `v[1]`, got: true"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":0,\"s\":0}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `z.s`, got: 0"), e.what());
  }
}

TEST(Serialization, StructSchemaSerialization) {
  using namespace serialization_test;
  using current::reflection::TypeID;
  using current::reflection::Reflector;
  using current::reflection::SchemaInfo;
  using current::reflection::StructSchema;
  using current::reflection::Language;

  StructSchema struct_schema;
  struct_schema.AddType<ComplexSerializable>();
  const std::string schema_json = JSON(struct_schema.GetSchemaInfo());

  // This, really, is just a golden sanity check. Can keep it this way for now. -- D.K.
  // clang-format off
  EXPECT_EQ(
      "{\"types\":[[\"T9000000000000000011\",{\"ReflectedType_Primitive\":{\"type_id\":\"T9000000000000000011\"},\"\":\"T9202934106479999325\"}],[\"T9000000000000000023\",{\"ReflectedType_Primitive\":{\"type_id\":\"T9000000000000000023\"},\"\":\"T9202934106479999325\"}],[\"T9000000000000000024\",{\"ReflectedType_Primitive\":{\"type_id\":\"T9000000000000000024\"},\"\":\"T9202934106479999325\"}],[\"T9000000000000000042\",{\"ReflectedType_Primitive\":{\"type_id\":\"T9000000000000000042\"},\"\":\"T9202934106479999325\"}],[\"T9010000002928410991\",{\"ReflectedType_Enum\":{\"type_id\":\"T9010000002928410991\",\"name\":\"Enum\",\"underlying_type\":\"T9000000000000000023\"},\"\":\"T9201951882596398273\"}],[\"T9201007113239016790\",{\"ReflectedType_Struct\":{\"type_id\":\"T9201007113239016790\",\"native_name\":\"Serializable\",\"super_id\":\"T1\",\"template_id\":null,\"fields\":[{\"type_id\":\"T9000000000000000024\",\"name\":\"i\",\"description\":null},{\"type_id\":\"T9000000000000000042\",\"name\":\"s\",\"description\":null},{\"type_id\":\"T9000000000000000011\",\"name\":\"b\",\"description\":null},{\"type_id\":\"T9010000002928410991\",\"name\":\"e\",\"description\":null}]},\"\":\"T9206858900297712816\"}],[\"T9209412029115735895\",{\"ReflectedType_Struct\":{\"type_id\":\"T9209412029115735895\",\"native_name\":\"ComplexSerializable\",\"super_id\":\"T1\",\"template_id\":null,\"fields\":[{\"type_id\":\"T9000000000000000024\",\"name\":\"j\",\"description\":null},{\"type_id\":\"T9000000000000000042\",\"name\":\"q\",\"description\":null},{\"type_id\":\"T9319767778871345491\",\"name\":\"v\",\"description\":null},{\"type_id\":\"T9201007113239016790\",\"name\":\"z\",\"description\":null}]},\"\":\"T9206858900297712816\"}],[\"T9319767778871345491\",{\"ReflectedType_Vector\":{\"type_id\":\"T9319767778871345491\",\"element_type\":\"T9000000000000000042\"},\"\":\"T9200962247788856851\"}]],\"order\":[\"T9319767778871345491\",\"T9010000002928410991\",\"T9201007113239016790\",\"T9209412029115735895\"]}",
      schema_json);
  // clang-format on

  const auto loaded_schema(ParseJSON<SchemaInfo>(schema_json));

  EXPECT_EQ(
      "namespace current_userspace {\n"
      "enum class Enum : uint32_t {};\n"
      "struct Serializable {\n"
      "  uint64_t i;\n"
      "  std::string s;\n"
      "  bool b;\n"
      "  Enum e;\n"
      "};\n"
      "struct ComplexSerializable {\n"
      "  uint64_t j;\n"
      "  std::string q;\n"
      "  std::vector<std::string> v;\n"
      "  Serializable z;\n"
      "};\n"
      "}  // namespace current_userspace\n",
      loaded_schema.Describe<Language::CPP>(false));
}

TEST(Serialization, JSONForCppTypes) {
  EXPECT_EQ("true", JSON(true));
  EXPECT_TRUE(ParseJSON<bool>("true"));
  ASSERT_THROW(ParseJSON<bool>("1"), JSONSchemaException);

  EXPECT_EQ("false", JSON(false));
  EXPECT_FALSE(ParseJSON<bool>("false"));
  ASSERT_THROW(ParseJSON<bool>("0"), JSONSchemaException);
  ASSERT_THROW(ParseJSON<bool>(""), InvalidJSONException);

  EXPECT_EQ("42", JSON(42));
  EXPECT_EQ(42, ParseJSON<int>("42"));

  EXPECT_EQ("\"forty two\"", JSON("forty two"));
  EXPECT_EQ("forty two", ParseJSON<std::string>("\"forty two\""));

  EXPECT_EQ("\"a\\u0000b\"", JSON(std::string("a\0b", 3)));
  EXPECT_EQ(std::string("c\0d", 3), ParseJSON<std::string>("\"c\\u0000d\""));

  EXPECT_EQ("[]", JSON(std::vector<uint64_t>()));
  EXPECT_EQ("[1,2,3]", JSON(std::vector<uint64_t>({1, 2, 3})));
  EXPECT_EQ("[[\"one\",\"two\"],[\"three\",\"four\"]]",
            JSON(std::vector<std::vector<std::string>>({{"one", "two"}, {"three", "four"}})));
  EXPECT_EQ(4u, ParseJSON<std::vector<std::vector<std::string>>>("[[],[],[],[]]").size());
  EXPECT_EQ("blah", ParseJSON<std::vector<std::vector<std::string>>>("[[],[\"\",\"blah\"],[],[]]")[1][1]);

  using map_int_int = std::map<int, int>;
  using map_string_int = std::map<std::string, int>;
  EXPECT_EQ("[]", JSON(map_int_int()));
  EXPECT_EQ("{}", JSON(map_string_int()));

  EXPECT_EQ(3u, ParseJSON<map_int_int>("[[2,4],[3,9],[4,16]]").size());
  EXPECT_EQ(16, ParseJSON<map_int_int>("[[2,4],[3,9],[4,16]]").at(4));
  ASSERT_THROW(ParseJSON<map_int_int>("{}"), JSONSchemaException);
  try {
    ParseJSON<map_int_int>("{}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ("Expected map as array, got: {}", std::string(e.what()));
  }

  EXPECT_EQ(2u, ParseJSON<map_string_int>("{\"a\":1,\"b\":2}").size());
  EXPECT_EQ(2, ParseJSON<map_string_int>("{\"a\":1,\"b\":2}").at("b"));
  ASSERT_THROW(ParseJSON<map_string_int>("[]"), JSONSchemaException);
  try {
    ParseJSON<map_string_int>("[]");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ("Expected map as object, got: []", std::string(e.what()));
  }
}

#if 0
// TODO(dkorolev): DIMA FIXME binary format.
TEST(Serialization, OptionalAsBinary) {
  using namespace serialization_test;

  const std::string tmp_file = current::FileSystem::GenTmpFileName();
  const auto tmp_file_remover = current::FileSystem::ScopedRmFile(tmp_file);
  {
    std::ofstream ofs(tmp_file);
    WithOptional with_optional;
    SaveIntoBinary(ofs, with_optional);

    with_optional.i = 42;
    SaveIntoBinary(ofs, with_optional);

    with_optional.b = true;
    SaveIntoBinary(ofs, with_optional);

    with_optional.i = nullptr;
    SaveIntoBinary(ofs, with_optional);
  }
  {
    std::ifstream ifs(tmp_file);
    const auto parsed_empty = LoadFromBinary<WithOptional>(ifs);
    ASSERT_FALSE(Exists(parsed_empty.i));
    ASSERT_FALSE(Exists(parsed_empty.b));

    const auto parsed_with_i = LoadFromBinary<WithOptional>(ifs);
    ASSERT_TRUE(Exists(parsed_with_i.i));
    ASSERT_FALSE(Exists(parsed_with_i.b));
    EXPECT_EQ(42, Value(parsed_with_i.i));

    const auto parsed_with_both = LoadFromBinary<WithOptional>(ifs);
    ASSERT_TRUE(Exists(parsed_with_both.i));
    ASSERT_TRUE(Exists(parsed_with_both.b));
    EXPECT_EQ(42, Value(parsed_with_both.i));
    EXPECT_TRUE(Value(parsed_with_both.b));

    const auto parsed_with_b = LoadFromBinary<WithOptional>(ifs);
    ASSERT_FALSE(Exists(parsed_with_b.i));
    ASSERT_TRUE(Exists(parsed_with_b.b));
    EXPECT_TRUE(Value(parsed_with_b.b));
  }
}
#endif

TEST(Serialization, OptionalAsJSON) {
  using namespace serialization_test;

  WithOptional with_optional;
  EXPECT_EQ("{\"i\":null,\"b\":null}", JSON(with_optional));
  {
    const auto parsed = ParseJSON<WithOptional>("{\"i\":null,\"b\":null}");
    ASSERT_FALSE(Exists(parsed.i));
    ASSERT_FALSE(Exists(parsed.b));
  }
  {
    const auto parsed = ParseJSON<WithOptional>("{}");
    ASSERT_FALSE(Exists(parsed.i));
    ASSERT_FALSE(Exists(parsed.b));
  }

  with_optional.i = 42;
  EXPECT_EQ("{\"i\":42,\"b\":null}", JSON(with_optional));
  {
    const auto parsed = ParseJSON<WithOptional>("{\"i\":42,\"b\":null}");
    ASSERT_TRUE(Exists(parsed.i));
    ASSERT_FALSE(Exists(parsed.b));
    EXPECT_EQ(42, Value(parsed.i));
  }
  {
    const auto parsed = ParseJSON<WithOptional>("{\"i\":42}");
    ASSERT_TRUE(Exists(parsed.i));
    ASSERT_FALSE(Exists(parsed.b));
    EXPECT_EQ(42, Value(parsed.i));
  }

  with_optional.b = true;
  EXPECT_EQ("{\"i\":42,\"b\":true}", JSON(with_optional));
  {
    const auto parsed = ParseJSON<WithOptional>("{\"i\":42,\"b\":true}");
    ASSERT_TRUE(Exists(parsed.i));
    ASSERT_TRUE(Exists(parsed.b));
    EXPECT_EQ(42, Value(parsed.i));
    EXPECT_TRUE(Value(parsed.b));
  }

  with_optional.i = nullptr;
  EXPECT_EQ("{\"i\":null,\"b\":true}", JSON(with_optional));
  {
    const auto parsed = ParseJSON<WithOptional>("{\"i\":null,\"b\":true}");
    ASSERT_FALSE(Exists(parsed.i));
    ASSERT_TRUE(Exists(parsed.b));
    EXPECT_TRUE(Value(parsed.b));
  }
  {
    const auto parsed = ParseJSON<WithOptional>("{\"b\":true}");
    ASSERT_FALSE(Exists(parsed.i));
    ASSERT_TRUE(Exists(parsed.b));
    EXPECT_TRUE(Value(parsed.b));
  }
}

TEST(Serialization, VariantAsJSON) {
  using namespace serialization_test;
  using namespace serialization_test::named_variant;

  {
    try {
      ParseJSON<simple_variant_t>("null");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (JSONUninitializedVariantObjectException) {
    }
  }
  {
    const simple_variant_t object = Empty();
    const std::string json = "{\"Empty\":{},\"\":\"T9200000002835747520\"}";
    EXPECT_EQ(json, JSON(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json, JSON(ParseJSON<simple_variant_t>(json)));
  }
  {
    const simple_variant_t object = Empty();
    const std::string json = "{\"Empty\":{}}";
    EXPECT_EQ(json, JSON<JSONFormat::Minimalistic>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json,
              JSON<JSONFormat::Minimalistic>(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(json)));
    EXPECT_EQ(json, JSON<JSONFormat::Minimalistic>(ParseJSON<simple_variant_t, JSONFormat::JavaScript>(json)));
  }
  {
    const simple_variant_t object = Empty();
    const std::string json = "{\"Empty\":{},\"$\":\"Empty\"}";
    EXPECT_EQ(json, JSON<JSONFormat::JavaScript>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json, JSON<JSONFormat::JavaScript>(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(json)));
    EXPECT_EQ(json, JSON<JSONFormat::JavaScript>(ParseJSON<simple_variant_t, JSONFormat::JavaScript>(json)));
  }
  {
    const simple_variant_t object = Empty();
    const std::string json = "{\"Case\":\"Empty\"}";
    EXPECT_EQ(json, JSON<JSONFormat::NewtonsoftFSharp>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(
        json,
        JSON<JSONFormat::NewtonsoftFSharp>(ParseJSON<simple_variant_t, JSONFormat::NewtonsoftFSharp>(json)));
  }
  {
    static_assert(IS_CURRENT_STRUCT(Empty), "");
    static_assert(IS_CURRENT_STRUCT(AlternativeEmpty), "");

    static_assert(IS_EMPTY_CURRENT_STRUCT(Empty), "");

    static_assert(!IS_EMPTY_CURRENT_STRUCT(simple_variant_t), "");
    static_assert(!IS_EMPTY_CURRENT_STRUCT(Serializable), "");

    const auto empty1 = ParseJSON<simple_variant_t, JSONFormat::NewtonsoftFSharp>("{\"Case\":\"Empty\"}");
    EXPECT_TRUE(Exists<Empty>(empty1));
    EXPECT_FALSE(Exists<AlternativeEmpty>(empty1));

    const auto empty2 =
        ParseJSON<simple_variant_t, JSONFormat::NewtonsoftFSharp>("{\"Case\":\"AlternativeEmpty\"}");
    EXPECT_FALSE(Exists<Empty>(empty2));
    EXPECT_TRUE(Exists<AlternativeEmpty>(empty2));
  }
  {
    const simple_variant_t object(Serializable(42));
    const std::string json =
        "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":\"T9201007113239016790\"}";
    EXPECT_EQ(json, JSON(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json, JSON(ParseJSON<simple_variant_t>(json)));
  }
  {
    const simple_variant_t object(Serializable(42));
    const std::string json = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0}}";
    EXPECT_EQ(json, JSON<JSONFormat::Minimalistic>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json,
              JSON<JSONFormat::Minimalistic>(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(json)));

    // An extra test that `Minimalistic` parser accepts the standard `Current` JSON format.
    EXPECT_EQ(JSON(object), JSON(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(json)));
    const std::string ok2 = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":false}";
    EXPECT_EQ(JSON(object), JSON(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(ok2)));
    const std::string ok3 = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":42}";
    EXPECT_EQ(JSON(object), JSON(ParseJSON<simple_variant_t, JSONFormat::Minimalistic>(ok3)));
  }
  {
    const simple_variant_t object(Serializable(42));
    const std::string json =
        "{\"Case\":\"Serializable\",\"Fields\":[{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0}]}";
    EXPECT_EQ(json, JSON<JSONFormat::NewtonsoftFSharp>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(
        json,
        JSON<JSONFormat::NewtonsoftFSharp>(ParseJSON<simple_variant_t, JSONFormat::NewtonsoftFSharp>(json)));
  }

  {
    WithOptional with_optional;
    with_optional.i = 42;
    InnerVariant inner_variant(with_optional);
    WithInnerVariant outer_variant;
    outer_variant.v = inner_variant;
    const std::string json = JSON(outer_variant);
    EXPECT_EQ("{\"v\":{\"WithOptional\":{\"i\":42,\"b\":null},\"\":\"T9202463557075072772\"}}", json);
    const WithInnerVariant parsed_object = ParseJSON<WithInnerVariant>(json);
    const WithOptional& inner_parsed_object = Value<WithOptional>(parsed_object.v);
    EXPECT_EQ(42, Value(inner_parsed_object.i));
    EXPECT_FALSE(Exists(inner_parsed_object.b));
  }
}

TEST(Serialization, NamedVariantAsJSON) {
  using namespace serialization_test::named_variant;

  {
    Q q;
    InnerA a;
    X x;
    OuterA aa;
    aa.a = x;
    q = aa;

    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(X), "");
    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(InnerA), "");
    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(InnerB), "");
    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(OuterA), "");
    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(OuterB), "");
    static_assert(IS_CURRENT_STRUCT_OR_VARIANT(Q), "");

    static_assert(IS_CURRENT_STRUCT(X), "");
    static_assert(!IS_CURRENT_VARIANT(X), "");

    static_assert(!IS_CURRENT_STRUCT(InnerA), "");
    static_assert(IS_CURRENT_VARIANT(InnerA), "");

    static_assert(IS_CURRENT_STRUCT(OuterA), "");
    static_assert(!IS_CURRENT_VARIANT(OuterA), "");

    static_assert(!IS_CURRENT_STRUCT(Q), "");
    static_assert(IS_CURRENT_VARIANT(Q), "");

    const auto json = JSON(q);
    EXPECT_EQ(
        "{\"OuterA\":{\"a\":{\"X\":{\"x\":1},\"\":\"T9209980946934124423\"}},\"\":\"T9204184145839071965\"}",
        json);

    const auto result = ParseJSON<Q>(json);
    ASSERT_TRUE(Exists<OuterA>(result));
    EXPECT_FALSE(Exists<OuterB>(result));
    ASSERT_TRUE(Exists<X>(Value<OuterA>(result).a));
    EXPECT_FALSE(Exists<Y>(Value<OuterA>(result).a));
    EXPECT_EQ(1, Value<X>(Value<OuterA>(result).a).x);
  }

  {
    Q q;
    InnerA a;
    Y y;
    OuterA aa;
    aa.a = y;
    q = aa;

    const auto json = JSON<JSONFormat::Minimalistic>(q);
    EXPECT_EQ("{\"OuterA\":{\"a\":{\"Y\":{\"y\":2}}}}", json);

    const auto result = ParseJSON<Q, JSONFormat::Minimalistic>(json);
    ASSERT_TRUE(Exists<OuterA>(result));
    EXPECT_FALSE(Exists<OuterB>(result));
    ASSERT_TRUE(Exists<Y>(Value<OuterA>(result).a));
    EXPECT_FALSE(Exists<X>(Value<OuterA>(result).a));
    EXPECT_EQ(2, Value<Y>(Value<OuterA>(result).a).y);
  }

  {
    Q q;
    InnerB b;
    Z z;
    OuterB bb;
    bb.b = z;
    q = bb;

    const auto json = JSON<JSONFormat::NewtonsoftFSharp>(q);
    EXPECT_EQ("{\"Case\":\"OuterB\",\"Fields\":[{\"b\":{\"Case\":\"Z\",\"Fields\":[{\"z\":3}]}}]}", json);

    const auto result = ParseJSON<Q, JSONFormat::NewtonsoftFSharp>(json);
    ASSERT_FALSE(Exists<OuterA>(result));
    EXPECT_TRUE(Exists<OuterB>(result));
    ASSERT_TRUE(Exists<Z>(Value<OuterB>(result).b));
    EXPECT_FALSE(Exists<T>(Value<OuterB>(result).b));
    EXPECT_EQ(3, Value<Z>(Value<OuterB>(result).b).z);
  }
}

TEST(Serialization, PairsInNewtonsoftJSONFSharpFormat) {
  auto a = std::make_pair(1, 2);
  EXPECT_EQ("[1,2]", JSON(a));
  EXPECT_EQ("{\"Item1\":1,\"Item2\":2}", JSON<JSONFormat::NewtonsoftFSharp>(a));
  EXPECT_EQ(JSON(a), JSON(ParseJSON<decltype(a)>(JSON(a))));
  EXPECT_EQ(JSON(a),
            JSON(ParseJSON<decltype(a), JSONFormat::NewtonsoftFSharp>(JSON<JSONFormat::NewtonsoftFSharp>(a))));
};

TEST(Serialization, OptionalInVariousFormats) {
  using namespace serialization_test;

  WithOptional object;
  EXPECT_EQ("{}", JSON<JSONFormat::Minimalistic>(object));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::Minimalistic>("{}").i));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::Minimalistic>("{}").b));

  object.i = 42;
  EXPECT_EQ("{\"i\":42}", JSON<JSONFormat::Minimalistic>(object));
  EXPECT_TRUE(Exists(ParseJSON<WithOptional, JSONFormat::Minimalistic>("{\"i\":42}").i));
  EXPECT_EQ(42, Value(ParseJSON<WithOptional, JSONFormat::Minimalistic>("{\"i\":42}").i));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::Minimalistic>("{}").b));

  object.i = 100;
  const std::string fsharp = "{\"i\":{\"Case\":\"Some\",\"Fields\":[100]}}";
  EXPECT_EQ(fsharp, JSON<JSONFormat::NewtonsoftFSharp>(object));
  EXPECT_TRUE(Exists(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>(fsharp).i));
  EXPECT_EQ(100, Value(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>(fsharp).i));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>("{}").b));
}

TEST(Serialization, LiberalOptionalForFSharp) {
  using serialization_test::WithOptional;

  EXPECT_FALSE(Exists(ParseJSON<Optional<bool>, JSONFormat::NewtonsoftFSharp>("{\"Case\":\"None\"}")));

  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>("{}").i));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>("{\"i\":null}").i));
  EXPECT_FALSE(Exists(ParseJSON<WithOptional, JSONFormat::NewtonsoftFSharp>("{\"i\":{\"Case\":\"None\"}}").i));

  ASSERT_THROW((ParseJSON<Optional<bool>, JSONFormat::NewtonsoftFSharp>("{\"Whatever\":\"None\"}")),
               JSONSchemaException);
  ASSERT_THROW((ParseJSON<Optional<bool>, JSONFormat::NewtonsoftFSharp>("{\"Case\":\"Whatever\"}")),
               JSONSchemaException);
}

TEST(Serialization, VariantNullOmittedInMinimalisticFormat) {
  using namespace serialization_test;

  EXPECT_EQ("{}", JSON<JSONFormat::Minimalistic>(ContainsVariant()));
  ParseJSON<ContainsVariant, JSONFormat::Minimalistic>("{}");
  EXPECT_FALSE(Exists(ParseJSON<ContainsVariant, JSONFormat::Minimalistic>("{}").variant));
  EXPECT_EQ("{\"variant\":null}", JSON(ContainsVariant()));

  EXPECT_EQ("null", JSON<JSONFormat::Minimalistic>(Variant<Empty>()));
  EXPECT_EQ("null", JSON(Variant<Empty>()));
}

TEST(Serialization, TimeAsJSON) {
  using namespace serialization_test;

  {
    WithTime zero;
    EXPECT_EQ("{\"number\":0,\"micros\":0}", JSON(zero));
  }

  {
    WithTime one;
    one.number = 1ull;
    one.micros = std::chrono::microseconds(2);
    EXPECT_EQ("{\"number\":1,\"micros\":2}", JSON(one));
  }

  {
    const auto parsed = ParseJSON<WithTime>("{\"number\":3,\"micros\":4}");
    EXPECT_EQ(3ull, parsed.number);
    EXPECT_EQ(4ll, parsed.micros.count());
  }
}

#if 0
// TODO(dkorolev): DIMA FIXME binary format.
TEST(Serialization, TimeAsBinary) {
  using namespace serialization_test;

  {
    WithTime zero;
    std::ostringstream oss;
    SaveIntoBinary(oss, zero);
    EXPECT_EQ(16u, oss.str().length());
  }

  {
    WithTime one;
    one.number = 5ull;
    one.micros = std::chrono::microseconds(6);
    std::ostringstream oss;
    SaveIntoBinary(oss, one);
    std::istringstream iss(oss.str());
    const auto parsed = LoadFromBinary<WithTime>(iss);
    EXPECT_EQ(5ull, parsed.number);
    EXPECT_EQ(6ll, parsed.micros.count());
  }
}
#endif

namespace serialization_test {

CURRENT_STRUCT_T(TemplatedValue) {
  CURRENT_FIELD(value, T);
  CURRENT_DEFAULT_CONSTRUCTOR_T(TemplatedValue) : value() {}
  CURRENT_CONSTRUCTOR_T(TemplatedValue)(const T& value) : value(value) {}
};

CURRENT_STRUCT(SimpleTemplatedUsage) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(t, TemplatedValue<std::string>);
};

CURRENT_STRUCT_T(ComplexTemplatedUsage) {
  CURRENT_FIELD(a, T);
  CURRENT_FIELD(b, TemplatedValue<T>);
};

CURRENT_STRUCT(DummyBaseClass) {
  CURRENT_FIELD(base, int32_t);
  CURRENT_CONSTRUCTOR(DummyBaseClass)(int32_t base = 0) : base(base) {}
};

// TODO(dkorolev): Test `: SUPER(42)` does the job.
CURRENT_STRUCT_T(DerivedTemplatedValue, DummyBaseClass) {
  CURRENT_FIELD(derived, T);
  CURRENT_DEFAULT_CONSTRUCTOR_T(DerivedTemplatedValue) : derived() {}
  CURRENT_CONSTRUCTOR_T(DerivedTemplatedValue)(const T& derived) : derived(derived) {}
};

}  // namespace serialization_test

TEST(Serialization, TemplatedValue) {
  using namespace serialization_test;

  EXPECT_EQ("{\"value\":1}", JSON(TemplatedValue<uint64_t>(1)));
  EXPECT_EQ("{\"value\":true}", JSON(TemplatedValue<bool>(true)));
  EXPECT_EQ("{\"value\":\"foo\"}", JSON(TemplatedValue<std::string>("foo")));
  EXPECT_EQ("{\"value\":{\"i\":1,\"s\":\"one\",\"b\":false,\"e\":0}}",
            JSON(TemplatedValue<Serializable>(Serializable(1, "one", false, Enum::DEFAULT))));

  EXPECT_EQ(42ull, ParseJSON<TemplatedValue<uint64_t>>("{\"value\":42}").value);
  EXPECT_TRUE(ParseJSON<TemplatedValue<bool>>("{\"value\":true}").value);
  EXPECT_EQ("ok", ParseJSON<TemplatedValue<std::string>>("{\"value\":\"ok\"}").value);
  EXPECT_EQ(100ull,
            ParseJSON<TemplatedValue<Serializable>>("{\"value\":{\"i\":100,\"s\":\"one\",\"b\":false,\"e\":0}}")
                .value.i);

  EXPECT_EQ("{\"base\":0,\"derived\":1}", JSON(DerivedTemplatedValue<uint64_t>(1)));
  EXPECT_EQ("{\"base\":0,\"derived\":true}", JSON(DerivedTemplatedValue<bool>(true)));
  EXPECT_EQ("{\"base\":0,\"derived\":\"foo\"}", JSON(DerivedTemplatedValue<std::string>("foo")));
  EXPECT_EQ("{\"base\":0,\"derived\":{\"i\":1,\"s\":\"one\",\"b\":false,\"e\":0}}",
            JSON(DerivedTemplatedValue<Serializable>(Serializable(1, "one", false, Enum::DEFAULT))));

  EXPECT_EQ(42ull, ParseJSON<DerivedTemplatedValue<uint64_t>>("{\"base\":1,\"derived\":42}").derived);
  EXPECT_EQ(43, ParseJSON<DerivedTemplatedValue<uint64_t>>("{\"base\":43,\"derived\":0}").base);
  EXPECT_TRUE(ParseJSON<DerivedTemplatedValue<bool>>("{\"base\":1,\"derived\":true}").derived);
  EXPECT_EQ("ok", ParseJSON<DerivedTemplatedValue<std::string>>("{\"base\":1,\"derived\":\"ok\"}").derived);
  EXPECT_EQ(43, ParseJSON<DerivedTemplatedValue<std::string>>("{\"base\":43,\"derived\":\"meh\"}").base);
  EXPECT_EQ(100ull,
            ParseJSON<DerivedTemplatedValue<Serializable>>(
                "{\"base\":1,\"derived\":{\"i\":100,\"s\":\"one\",\"b\":false,\"e\":0}}").derived.i);
  EXPECT_EQ(43,
            ParseJSON<DerivedTemplatedValue<Serializable>>(
                "{\"base\":43,\"derived\":{\"i\":1,\"s\":\"\",\"b\":true,\"e\":0}}").base);
}

TEST(Serialization, SimpleTemplatedUsage) {
  using namespace serialization_test;

  SimpleTemplatedUsage object;
  object.i = 42;
  object.t.value = "test";
  EXPECT_EQ("{\"i\":42,\"t\":{\"value\":\"test\"}}", JSON(object));

  const auto result = ParseJSON<SimpleTemplatedUsage>("{\"i\":100,\"t\":{\"value\":\"passed\"}}");
  EXPECT_EQ(100ull, result.i);
  EXPECT_EQ("passed", result.t.value);
}

TEST(Serialization, ComplexTemplatedUsage) {
  using namespace serialization_test;

  {
    ComplexTemplatedUsage<int32_t> object;
    object.a = 1;
    object.b.value = 2;
    EXPECT_EQ("{\"a\":1,\"b\":{\"value\":2}}", JSON(object));

    const auto result = ParseJSON<ComplexTemplatedUsage<int32_t>>("{\"a\":3,\"b\":{\"value\":4}}");
    EXPECT_EQ(3, result.a);
    EXPECT_EQ(4, result.b.value);
  }

  {
    ComplexTemplatedUsage<std::string> object;
    object.a = "x";
    object.b.value = "y";
    EXPECT_EQ("{\"a\":\"x\",\"b\":{\"value\":\"y\"}}", JSON(object));

    const auto result = ParseJSON<ComplexTemplatedUsage<std::string>>("{\"a\":\"z\",\"b\":{\"value\":\"t\"}}");
    EXPECT_EQ("z", result.a);
    EXPECT_EQ("t", result.b.value);
  }
}

TEST(Serialization, VariantWithTemplatedStructs) {
  using namespace serialization_test;
  using complex_variant_t = Variant<ComplexTemplatedUsage<int32_t>, ComplexTemplatedUsage<std::string>>;

  {
    using inner_object_t = ComplexTemplatedUsage<int32_t>;
    complex_variant_t object;
    inner_object_t contents;
    contents.a = 1;
    contents.b.value = 2;
    object = contents;
    EXPECT_EQ("{\"ComplexTemplatedUsage_Z\":{\"a\":1,\"b\":{\"value\":2}},\"\":\"T9203223964691052963\"}",
              JSON(object));

    const auto result = ParseJSON<complex_variant_t>(
        "{\"ComplexTemplatedUsage_Z\":{\"a\":1,\"b\":{\"value\":2}},"
        "\"\":\"T9203223964691052963\"}");
    ASSERT_TRUE(Exists<inner_object_t>(result));
    const auto& parsed_object = Value<inner_object_t>(result);
    EXPECT_EQ(1, parsed_object.a);
    EXPECT_EQ(2, parsed_object.b.value);
  }

  {
    using inner_object_t = ComplexTemplatedUsage<std::string>;
    complex_variant_t object;
    inner_object_t contents;
    contents.a = "yes";
    contents.b.value = "no";
    object = contents;
    EXPECT_EQ(
        "{\"ComplexTemplatedUsage_Z\":{\"a\":\"yes\",\"b\":{\"value\":\"no\"}},\"\":\"T9203187966977056754\"}",
        JSON(object));

    const auto result = ParseJSON<complex_variant_t>(
        "{\"ComplexTemplatedUsage_Z\":{\"a\":\"yes\",\"b\":{\"value\":\"no\"}},"
        "\"\":\"T9203187966977056754\"}");
    ASSERT_TRUE(Exists<inner_object_t>(result));
    const auto& parsed_object = Value<inner_object_t>(result);
    EXPECT_EQ("yes", parsed_object.a);
    EXPECT_EQ("no", parsed_object.b.value);
  }
}

namespace serialization_test {

CURRENT_ENUM(CrashingEnum, uint64_t){};

CURRENT_STRUCT(CrashingStruct) {
  CURRENT_FIELD(i, int64_t, 0);
  CURRENT_FIELD(o, Optional<int64_t>);
  CURRENT_FIELD(e, CrashingEnum, static_cast<CrashingEnum>(0));
};

}  // namespace serialization_test

TEST(Serialization, JSONCrashTests) {
  EXPECT_EQ("{\"i\":0,\"o\":null,\"e\":0}", JSON(serialization_test::CrashingStruct()));

  {
    // Attempt to fit `0.5` into an `int64_t`.
    try {
      ParseJSON<serialization_test::CrashingStruct>("{\"i\":0.5,\"o\":null,\"e\":0}");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected integer for `i`, got: 0.5"), e.what());
    }
#if 0
    catch (const RapidJSONAssertionFailedException& e) {
      ExpectStringEndsWith("data_.f.flags & kInt64Flag\tdata_.f.flags & kInt64Flag", e.What());
    }
#endif
  }

  {
    // Attempt to fit `0.5` into an `Optional<int64_t>`.
    try {
      ParseJSON<serialization_test::CrashingStruct>("{\"i\":0,\"o\":0.5,\"e\":0}");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected integer for `o`, got: 0.5"), e.what());
    }
#if 0
    catch (const RapidJSONAssertionFailedException& e) {
      ExpectStringEndsWith("data_.f.flags & kInt64Flag\tdata_.f.flags & kInt64Flag", e.What());
    }
#endif
  }

  {
    // Attempt to fit `0.5` into a `CURRENT_ENUM(*, uint64_t)`.
    try {
      ParseJSON<serialization_test::CrashingStruct>("{\"i\":0,\"o\":null,\"e\":0.5}");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected enum as signed integer for `e`, got: 0.5"), e.what());
    }
  }
}

namespace serialization_test {
CURRENT_STRUCT(StructToPatch) {
  CURRENT_FIELD(a, int64_t, 0);
  CURRENT_FIELD(b, std::string);
  CURRENT_FIELD(c, Optional<std::string>);
};
}  // namespace serialization_test

TEST(Serialization, PatchObjectWithJSON) {
  using serialization_test::StructToPatch;

  auto s = ParseJSON<StructToPatch>("{\"a\":1,\"b\":\"one\",\"c\":null}");
  EXPECT_EQ(1, s.a);
  EXPECT_EQ("one", s.b);
  EXPECT_FALSE(Exists(s.c));

  PatchObjectWithJSON(s, "{}");
  EXPECT_EQ(1, s.a);
  EXPECT_EQ("one", s.b);
  EXPECT_FALSE(Exists(s.c));

  PatchObjectWithJSON(s, "{\"a\":2}");
  EXPECT_EQ(2, s.a);
  EXPECT_EQ("one", s.b);
  EXPECT_FALSE(Exists(s.c));

  PatchObjectWithJSON(s, "{\"b\":\"two\"}");
  EXPECT_EQ(2, s.a);
  EXPECT_EQ("two", s.b);
  EXPECT_FALSE(Exists(s.c));

  PatchObjectWithJSON(s, "{\"c\":\"test\"}");
  EXPECT_EQ(2, s.a);
  EXPECT_EQ("two", s.b);
  EXPECT_TRUE(Exists(s.c));
  EXPECT_EQ("test", Value(s.c));

  PatchObjectWithJSON(s, "{}");
  EXPECT_TRUE(Exists(s.c));
  EXPECT_EQ("test", Value(s.c));

  PatchObjectWithJSON(s, "{\"c\":null}");
  EXPECT_FALSE(Exists(s.c));
}

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TEST_CC
