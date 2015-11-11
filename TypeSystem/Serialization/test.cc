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

#include "json.h"

#include "../Reflection/reflection.h"
#include "../Reflection/schema.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace serialization_test {

CURRENT_STRUCT(Serializable) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(s, std::string);

  // Note: The user has to define default constructor when defining custom ones.
  CURRENT_DEFAULT_CONSTRUCTOR(Serializable) {}
  CURRENT_CONSTRUCTOR(Serializable)(int i, const std::string& s) : i(i), s(s) {}

  CURRENT_RETURNS(uint64_t) twice_i() const { return i + i; }

  CURRENT_RETURNS(bool)operator<(const Serializable& rhs) const { return i < rhs.i; }
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

  CURRENT_RETURNS(size_t) length_of_v() const { return v.size(); }
};

CURRENT_STRUCT(WithVectorOfPairs) {
  using pair_int32_string = std::pair<int32_t, std::string>;
  CURRENT_FIELD(v, std::vector<pair_int32_string>);
};

CURRENT_STRUCT(WithTrivialMap) {
  using map_string_string = std::map<std::string, std::string>;  // Sigh. -- D.K.
  CURRENT_FIELD(m, map_string_string);
};

CURRENT_STRUCT(WithNontrivialMap) {
  // To dig into: http://stackoverflow.com/questions/13842468/comma-in-c-c-macro
  using map_serializable_string = std::map<Serializable, std::string>;  // Sigh. -- D.K.
  CURRENT_FIELD(q, map_serializable_string);
};

enum class MyMagicGUID : uint64_t {};
CURRENT_STRUCT(WithEnumClass) {
  CURRENT_FIELD(id1, MyMagicGUID);
  CURRENT_FIELD(id2, MyMagicGUID, static_cast<MyMagicGUID>(1));
};

}  // namespace serialization_test

TEST(Serialization, JSON) {
  using namespace serialization_test;

  // Simple serialization.
  Serializable simple_object;
  simple_object.i = 0;
  simple_object.s = "";

  EXPECT_EQ("{\"i\":0,\"s\":\"\"}", JSON(simple_object));

  simple_object.i = 42;
  simple_object.s = "foo";
  const std::string simple_object_as_json = JSON(simple_object);
  EXPECT_EQ("{\"i\":42,\"s\":\"foo\"}", simple_object_as_json);

  {
    Serializable a = ParseJSON<Serializable>(simple_object_as_json);
    EXPECT_EQ(42ull, a.i);
    EXPECT_EQ("foo", a.s);
  }

  // Nested serialization.
  ComplexSerializable complex_object;
  complex_object.j = 43;
  complex_object.q = "bar";
  complex_object.v.push_back("one");
  complex_object.v.push_back("two");
  complex_object.z = simple_object;

  const std::string complex_object_as_json = JSON(complex_object);
  EXPECT_EQ("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":42,\"s\":\"foo\"}}",
            complex_object_as_json);

  {
    ComplexSerializable b = ParseJSON<ComplexSerializable>(complex_object_as_json);
    EXPECT_EQ(43ull, b.j);
    EXPECT_EQ("bar", b.q);
    ASSERT_EQ(2u, b.v.size());
    EXPECT_EQ("one", b.v[0]);
    EXPECT_EQ("two", b.v[1]);
    EXPECT_EQ(42ull, b.z.i);
    EXPECT_EQ("foo", b.z.s);

    ASSERT_THROW(ParseJSON<ComplexSerializable>("not a json"), InvalidJSONException);
  }

  // Complex serialization makes a copy.
  simple_object.i = 1000;
  EXPECT_EQ(42ull, complex_object.z.i);
  EXPECT_EQ("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":42,\"s\":\"foo\"}}",
            JSON(complex_object));

  // Serializitaion/deserialization of `std::vector<std::pair<...>>`.
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
      ASSERT_TRUE(false);
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
    with_nontrivial_map.q[simple_object] = "wow";
    EXPECT_EQ("{\"q\":[[{\"i\":1000,\"s\":\"foo\"},\"wow\"]]}", JSON(with_nontrivial_map));
    with_nontrivial_map.q[Serializable(1, "one")] = "yes";
    EXPECT_EQ("{\"q\":[[{\"i\":1,\"s\":\"one\"},\"yes\"],[{\"i\":1000,\"s\":\"foo\"},\"wow\"]]}",
              JSON(with_nontrivial_map));
  }
  {
    const auto parsed = ParseJSON<WithNontrivialMap>("{\"q\":[]}");
    ASSERT_TRUE(parsed.q.empty());
  }
  {
    try {
      ParseJSON<WithNontrivialMap>("{\"q\":{}}");
      ASSERT_TRUE(false);
    } catch (const JSONSchemaException& e) {
      EXPECT_EQ(std::string("Expected map as array for `q`, got: {}"), e.what());
    }
  }
  {
    const auto parsed = ParseJSON<WithNontrivialMap>(
        "{\"q\":[[{\"i\":3,\"s\":\"three\"},\"prime\"],[{\"i\":4,\"s\":\"four\"},\"composite\"]]}");
    ASSERT_EQ(2u, parsed.q.size());
    EXPECT_EQ("prime", parsed.q.at(Serializable(3, "")));
    EXPECT_EQ("composite", parsed.q.at(Serializable(4, "")));
  }

  {
    WithEnumClass with_enum_class;
    EXPECT_EQ(1u, static_cast<uint64_t>(with_enum_class.id2));
    with_enum_class.id1 = static_cast<MyMagicGUID>(101);
    with_enum_class.id2 = static_cast<MyMagicGUID>(102);
    EXPECT_EQ("{\"id1\":101,\"id2\":102}", JSON(with_enum_class));

    const auto result = ParseJSON<WithEnumClass>("{\"id1\":201,\"id2\":202}");
    EXPECT_EQ(201u, static_cast<uint64_t>(result.id1));
    EXPECT_EQ(202u, static_cast<uint64_t>(result.id2));
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
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected value for `i`, got: {}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":\"boo\"}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: \"boo\""), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":[]}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: []"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":{}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `i`, got: {}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":100}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected value for `s`, got: {\"i\":100}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":42}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: 42"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":[]}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: []"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":{}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `s`, got: {}"), e.what());
  }

  // Names of inner, nested, fields.
  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":\"error\",\"s\":\"foo\"}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `z.i`, got: \"error\""), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":null,\"s\":\"foo\"}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number for `z.i`, got: null"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"s\":\"foo\"}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected value for `z.i`, got: {\"s\":\"foo\"}"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",true],\"z\":{\"i\":0,\"s\":0}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `v[1]`, got: true"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":0,\"s\":0}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string for `z.s`, got: 0"), e.what());
  }
}

TEST(Serialization, StructSchema) {
  using namespace serialization_test;
  using current::reflection::Reflector;
  using current::reflection::StructSchema;
  using current::reflection::ReflectedType_Struct;

  EXPECT_EQ(
      "{\"type_id\":9209545069565467078,\"name\":\"ComplexSerializable\",\"super_type_id\":0,\"fields\":[["
      "9000000000000000014,\"j\"],[9000000000000000101,\"q\"],[9310000000000000202,\"v\"],[9207121101994642474,"
      "\"z\"]]}",
      JSON(StructSchema(Reflector().ReflectType<ComplexSerializable>())));
}

// TODO(dkorolev): Move this test outside `Serialization`.
TEST(NotReallySerialization, ConstructorsAndMemberFunctions) {
  using namespace serialization_test;
  {
    Serializable simple_object(1, "foo");
    EXPECT_EQ(2u, simple_object.twice_i());
  }
  {
    ComplexSerializable complex_object('a', 'c');
    EXPECT_EQ(3u, complex_object.length_of_v());
  }
}

TEST(Serialization, JSONForCppTypes) {
  // RapidJSON requires the top-level node to be an array or object.
  // Thus, just `JSON(42)` or `JSON("foo")` doesn't work. But arrays do. ;-)
  EXPECT_EQ("true", JSON(true));
  EXPECT_TRUE(ParseJSON<bool>("true"));
  EXPECT_TRUE(ParseJSON<bool>("1"));

  EXPECT_EQ("false", JSON(false));
  EXPECT_FALSE(ParseJSON<bool>("false"));
  EXPECT_FALSE(ParseJSON<bool>("0"));
  EXPECT_FALSE(ParseJSON<bool>(""));

  EXPECT_EQ("42", JSON(42));
  EXPECT_EQ(42, ParseJSON<int>("42"));

  EXPECT_EQ("forty two", JSON("forty two"));
  EXPECT_EQ("forty two", ParseJSON<std::string>("forty two"));

  EXPECT_EQ(std::string("a\0b", 3), JSON(std::string("a\0b", 3)));
  EXPECT_EQ(std::string("c\0d", 3), ParseJSON<std::string>(std::string("c\0d", 3)));

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
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ("Expected map as array, got: {}", std::string(e.what()));
  }

  EXPECT_EQ(2u, ParseJSON<map_string_int>("{\"a\":1,\"b\":2}").size());
  EXPECT_EQ(2, ParseJSON<map_string_int>("{\"a\":1,\"b\":2}").at("b"));
  ASSERT_THROW(ParseJSON<map_string_int>("[]"), JSONSchemaException);
  try {
    ParseJSON<map_string_int>("[]");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ("Expected map as object, got: []", std::string(e.what()));
  }
}

// RapidJSON examples framed as tests. One day we may wish to remove them. -- D.K.
TEST(RapidJSON, Smoke) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::GenericWriteStream;

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
    auto stream = GenericWriteStream(os);
    auto writer = Writer<GenericWriteStream>(stream);
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
  using rapidjson::GenericWriteStream;

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
    auto stream = GenericWriteStream(os);
    auto writer = Writer<GenericWriteStream>(stream);
    document.Accept(writer);
    json = os.str();
  }

  EXPECT_EQ("[42,\"bar\"]", json);
}

TEST(RapidJSON, NullInString) {
  using rapidjson::Document;
  using rapidjson::Value;
  using rapidjson::Writer;
  using rapidjson::GenericWriteStream;

  std::string json;

  {
    Document document;
    auto& allocator = document.GetAllocator();
    Value s;
    s.SetString("terrible\0avoided", strlen("terrible") + 1 + strlen("avoided"));
    document.SetObject();
    document.AddMember("s", s, allocator);

    std::ostringstream os;
    auto stream = GenericWriteStream(os);
    auto writer = Writer<GenericWriteStream>(stream);
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
