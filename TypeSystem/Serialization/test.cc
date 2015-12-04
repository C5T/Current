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

}  // namespace serialization_test

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
    EXPECT_EQ(std::string("Expected number for `i`, got: missing field."), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":\"boo\"}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: \"boo\""), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":[]}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: []"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":{}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: {}"), e.what());
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
    EXPECT_EQ(std::string("Expected number for `z.i`, got: \"error\""), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":null,\"s\":\"foo\"}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `z.i`, got: null"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"s\":\"foo\"}}");
    ASSERT_TRUE(false);  // LCOV_EXCL_LINE
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `z.i`, got: missing field."), e.what());
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

  EXPECT_EQ(
      "{\"types\":[[\"T9000000000000000011\",{\"ReflectedType_Primitive\":{\"type_id\":"
      "\"T9000000000000000011\"},\"\":\"T9200000000887757410\"}],[\"T9000000000000000023\",{\"ReflectedType_"
      "Primitive\":{\"type_id\":\"T9000000000000000023\"},\"\":\"T9200000000887757410\"}],["
      "\"T9000000000000000024\",{\"ReflectedType_Primitive\":{\"type_id\":\"T9000000000000000024\"},\"\":"
      "\"T9200000000887757410\"}],[\"T9000000000000000042\",{\"ReflectedType_Primitive\":{\"type_id\":"
      "\"T9000000000000000042\"},\"\":\"T9200000000887757410\"}],[\"T9010000002928410991\",{\"ReflectedType_"
      "Enum\":{\"type_id\":\"T9010000002928410991\",\"name\":\"Enum\",\"underlying_type\":"
      "\"T9000000000000000023\"},\"\":\"T9200284389866084350\"}],[\"T9201007113239016790\",{\"ReflectedType_"
      "Struct\":{\"type_id\":\"T9201007113239016790\",\"name\":\"Serializable\",\"super_id\":\"T1\",\"fields\":"
      "[[\"T9000000000000000024\",\"i\"],[\"T9000000000000000042\",\"s\"],[\"T9000000000000000011\",\"b\"],["
      "\"T9010000002928410991\",\"e\"]]},\"\":\"T9204383916472223645\"}],[\"T9209412029115735895\",{"
      "\"ReflectedType_Struct\":{\"type_id\":\"T9209412029115735895\",\"name\":\"ComplexSerializable\",\"super_"
      "id\":\"T1\",\"fields\":[[\"T9000000000000000024\",\"j\"],[\"T9000000000000000042\",\"q\"],["
      "\"T9319767778871345491\",\"v\"],[\"T9201007113239016790\",\"z\"]]},\"\":\"T9204383916472223645\"}],["
      "\"T9319767778871345491\",{\"ReflectedType_Vector\":{\"type_id\":\"T9319767778871345491\",\"element_"
      "type\":\"T9000000000000000042\"},\"\":\"T9209585172575626540\"}]],\"order\":[\"T9319767778871345491\","
      "\"T9010000002928410991\",\"T9201007113239016790\",\"T9209412029115735895\"]}",
      schema_json);

  const SchemaInfo loaded_schema(ParseJSON<SchemaInfo>(schema_json));

  EXPECT_EQ(
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
      "};\n",
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
  using RequiredVariantType = Variant<Empty, Serializable, ComplexSerializable>;
  {
    try {
      ParseJSON<RequiredVariantType>("null");
      ASSERT_TRUE(false);  // LCOV_EXCL_LINE
    } catch (JSONUninitializedVariantObjectException) {
    }
  }
  {
    const RequiredVariantType object(make_unique<Empty>());
    const std::string json = "{\"Empty\":{},\"\":\"T9200000002835747520\"}";
    EXPECT_EQ(json, JSON(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json, JSON(ParseJSON<RequiredVariantType>(json)));
  }
  {
    const RequiredVariantType object(make_unique<Empty>());
    const std::string json = "{\"Empty\":{}}";
    EXPECT_EQ(json, JSON<JSONFormat::Minimalistic>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json,
              JSON<JSONFormat::Minimalistic>(ParseJSON<RequiredVariantType, JSONFormat::Minimalistic>(json)));
  }
  {
    const RequiredVariantType object(make_unique<Empty>());
    const std::string json = "{\"Case\":\"Empty\",\"Fields\":[{}]}";
    EXPECT_EQ(json, JSON<JSONFormat::NewtonsoftFSharp>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(
        json,
        JSON<JSONFormat::NewtonsoftFSharp>(ParseJSON<RequiredVariantType, JSONFormat::NewtonsoftFSharp>(json)));
  }
  {
    const RequiredVariantType object(make_unique<Serializable>(42));
    const std::string json =
        "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":\"T9201007113239016790\"}";
    EXPECT_EQ(json, JSON(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json, JSON(ParseJSON<RequiredVariantType>(json)));
  }
  {
    const RequiredVariantType object(make_unique<Serializable>(42));
    const std::string json = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0}}";
    EXPECT_EQ(json, JSON<JSONFormat::Minimalistic>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(json,
              JSON<JSONFormat::Minimalistic>(ParseJSON<RequiredVariantType, JSONFormat::Minimalistic>(json)));

    // An extra test that `Minimalistic` parser accepts the standard `Current` JSON format.
    EXPECT_EQ(JSON(object), JSON(ParseJSON<RequiredVariantType, JSONFormat::Minimalistic>(json)));
    const std::string ok2 = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":false}";
    EXPECT_EQ(JSON(object), JSON(ParseJSON<RequiredVariantType, JSONFormat::Minimalistic>(ok2)));
    const std::string ok3 = "{\"Serializable\":{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0},\"\":42}";
    EXPECT_EQ(JSON(object), JSON(ParseJSON<RequiredVariantType, JSONFormat::Minimalistic>(ok3)));
  }
  {
    const RequiredVariantType object(make_unique<Serializable>(42));
    const std::string json =
        "{\"Case\":\"Serializable\",\"Fields\":[{\"i\":42,\"s\":\"\",\"b\":false,\"e\":0}]}";
    EXPECT_EQ(json, JSON<JSONFormat::NewtonsoftFSharp>(object));
    // Confirm that `ParseJSON()` does the job. Top-level `JSON()` is just to simplify the comparison.
    EXPECT_EQ(
        json,
        JSON<JSONFormat::NewtonsoftFSharp>(ParseJSON<RequiredVariantType, JSONFormat::NewtonsoftFSharp>(json)));
  }

  // TODO(dk+mz): This should compile. And it compiles. Does it work properly?
  if (false) {
    using OtherRequiredVariantType = Variant<WithVectorOfPairs, WithOptional>;
    using WeHaveToGoDeeper = Variant<RequiredVariantType, OtherRequiredVariantType>;
    ParseJSON<WeHaveToGoDeeper>("This should compile.");
    WeHaveToGoDeeper* this_should_compile_too;
    JSON(*this_should_compile_too);
  }
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

// RapidJSON examples framed as tests. One day we may wish to remove them. -- D.K.
TEST(RapidJSON, Smoke) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::OStreamWrapper;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    Value foo("bar");
    document.SetObject().AddMember("foo", foo, allocator);

    EXPECT_TRUE(document.IsObject());
    EXPECT_FALSE(document.IsArray());
    EXPECT_TRUE(document.HasMember("foo"));
    EXPECT_TRUE(document["foo"].IsString());
    EXPECT_EQ("bar", document["foo"].GetString());

    std::ostringstream os;
    OStreamWrapper stream(os);
    Writer<OStreamWrapper> writer(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("{\"foo\":\"bar\"}", json);

  {
    Document document;
    ASSERT_FALSE(document.Parse<0>(json.c_str()).HasParseError());
    EXPECT_TRUE(document.IsObject());
    EXPECT_TRUE(document.HasMember("foo"));
    EXPECT_TRUE(document["foo"].IsString());
    EXPECT_EQ(std::string("bar"), document["foo"].GetString());
    EXPECT_FALSE(document.HasMember("bar"));
    EXPECT_FALSE(document.HasMember("meh"));
  }
}

TEST(RapidJSON, Array) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::OStreamWrapper;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    document.SetArray();
    Value element;
    element = 42;
    document.PushBack(element, allocator);
    element = "bar";
    document.PushBack(element, allocator);

    EXPECT_TRUE(document.IsArray());
    EXPECT_FALSE(document.IsObject());

    std::ostringstream os;
    OStreamWrapper stream(os);
    Writer<OStreamWrapper> writer(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("[42,\"bar\"]", json);
}

TEST(RapidJSON, NullInString) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::OStreamWrapper;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    Value s;
    s.SetString("terrible\0avoided", strlen("terrible") + 1 + strlen("avoided"));
    document.SetObject();
    document.AddMember("s", s, allocator);

    std::ostringstream os;
    OStreamWrapper stream(os);
    Writer<OStreamWrapper> writer(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("{\"s\":\"terrible\\u0000avoided\"}", json);

  {
    Document document;
    ASSERT_FALSE(document.Parse<0>(json.c_str()).HasParseError());
    EXPECT_EQ(std::string("terrible"), document["s"].GetString());
    EXPECT_EQ(std::string("terrible\0avoided", strlen("terrible") + 1 + strlen("avoided")),
              std::string(document["s"].GetString(), document["s"].GetStringLength()));
  }
}

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TEST_CC
