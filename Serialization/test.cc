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

#include "json.h"

#include "../3rdparty/gtest/gtest-main.h"

namespace serialization_test {

CURRENT_STRUCT(Serializable) {
  CURRENT_FIELD(i, uint64_t);
  CURRENT_FIELD(s, std::string);

  CONSTRUCTOR(int i, const std::string& s) {
    this->i = i;
    this->s = s;
  }

  RETURNS(int) twice_i() const { return i + i; }
};

CURRENT_STRUCT(ComplexSerializable) {
  CURRENT_FIELD(j, uint64_t);
  CURRENT_FIELD(q, std::string);
  CURRENT_FIELD(v, std::vector<std::string>);
  CURRENT_FIELD(z, Serializable);

  CONSTRUCTOR(char a, char b) {
    for (char c = a; c <= b; ++c) {
      v.push_back(std::string(1, c));
    }
  }

  RETURNS(size_t) length_of_v() const { return v.size(); }
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

    ASSERT_THROW(ParseJSON<ComplexSerializable>("not a json"), ParseJSONException);
  }

  // Complex serialization makes a copy.
  simple_object.i = -1000;
  EXPECT_EQ(42ull, complex_object.z.i);
  EXPECT_EQ("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":42,\"s\":\"foo\"}}",
            JSON(complex_object));
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
    EXPECT_EQ(std::string("Expected number, got: {\"i\":null}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":\"boo\"}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number, got: {\"i\":\"boo\"}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":[]}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number, got: {\"i\":[]}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":{}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number, got: {\"i\":{}}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string, got: {\"s\":null}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":42}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string, got: {\"s\":42}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":[]}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string, got: {\"s\":[]}"), e.what());
  }

  try {
    ParseJSON<Serializable>("{\"i\":42,\"s\":{}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string, got: {\"s\":{}}"), e.what());
  }

  // Names of inner, nested, fields.
  try {
    ParseJSON<ComplexSerializable>(
        "{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":\"error\",\"s\":\"foo\"}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected number, got: {\"z.i\":\"error\"}"), e.what());
  }

  try {
    ParseJSON<ComplexSerializable>("{\"j\":43,\"q\":\"bar\",\"v\":[\"one\",\"two\"],\"z\":{\"i\":0,\"s\":0}}");
    ASSERT_TRUE(false);
  } catch (const JSONSchemaException& e) {
    EXPECT_EQ(std::string("Expected string, got: {\"z.s\":0}"), e.what());
  }
}

// TODO(dkorolev): Move this test outside `Serialization`.
TEST(NotReallySerialization, ConstructorsAndMemberFunctions) {
  using namespace serialization_test;
  // TODO(dkorolev): This syntax should probably be clean up to:
  // Serializable simple_object = CURRENT_CONSTRUCT(1, "foo");
  {
    Serializable simple_object(Serializable::Construct(1, "foo"));
    EXPECT_EQ(2, simple_object.twice_i());
  }
  {
    ComplexSerializable complex_object(ComplexSerializable::Construct('a', 'c'));
    EXPECT_EQ(3u, complex_object.length_of_v());
  }
}

TEST(Serialization, LOLWUT) {
  // RapidJSON requires the top-level node to be an array or object.
  // Thus, just `JSON(42)` or `JSON("foo")` doesn't work. But arrays do. ;-)
  using namespace serialization_test;
  EXPECT_EQ("[1,2,3]", JSON(std::vector<uint64_t>({1, 2, 3})));
  EXPECT_EQ("[[\"one\",\"two\"],[\"three\",\"four\"]]",
            JSON(std::vector<std::vector<std::string>>({{"one", "two"}, {"three", "four"}})));
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
