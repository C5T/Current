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

// TODO(dkorolev): Human-readable messages in exceptions.
// TODO(dkorolev): Death tests.

#include "infer.h"

#include "../../Bricks/file/file.h"
#include "../../3rdparty/gtest/gtest-main.h"

// Smoke test cases.
TEST(InferJSONSchema, Empty) {
  EXPECT_EQ("{\"ObjectOrArray\":{\"fields\":{},\"nulls\":0}}",
            JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("{}")));
}

TEST(InferJSONSchema, OneStringField) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"a\":{\"String\":{\"values\":{\"b\":1},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("{\"a\":\"b\"}")));
}

TEST(InferJSONSchema, TwoStringFields) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"a\":{\"String\":{\"values\":{\"b\":1},\"nulls\":0}},\"c\":{\"String\":"
      "{\"values\":{\"d\":1},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("{\"a\":\"b\",\"c\":\"d\"}")));
}

TEST(InferJSONSchema, NestedObject) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"a\":{\"ObjectOrArray\":{\"fields\":{\"b\":{\"String\":{\"values\":{"
      "\"c\":1},\"nulls\":0}}},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("{\"a\":{\"b\":\"c\"}}")));
}

TEST(InferJSONSchema, ArrayOfOneString) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"String\":{\"values\":{\"a\":1},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("[\"a\"]")));
}
TEST(InferJSONSchema, ArrayOfOneEmptyObject) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("[{}]")));
}

TEST(InferJSONSchema, ArrayOfThreeStrings) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"String\":{\"values\":{\"a\":2,\"b\":1},\"nulls\":0}}},\"nulls\":"
      "0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("[\"a\",\"b\",\"a\"]")));
}

TEST(InferJSONSchema, ArrayOfThreeComparableObjects) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{\"x\":{\"String\":{\"values\":{"
      "\"a\":2,\"b\":1},\"nulls\":0}}},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(
          current::utils::InferRawSchemaFromJSON("[{\"x\":\"a\"},{\"x\":\"b\"},{\"x\":\"a\"}]")));
}

TEST(InferJSONSchema, ArrayOfIncomparableObjects) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{\"x\":{\"String\":{\"values\":{"
      "\"a\":1},\"nulls\":1}},\"y\":{\"String\":{\"values\":{\"b\":1},\"nulls\":1}}},\"nulls\":0}}},\"nulls\":"
      "0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("[{\"x\":\"a\"},{\"y\":\"b\"}]")));
}

TEST(InferJSONSchema, ArrayWithEmptyObjects) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{\"p\":{\"String\":{\"values\":{"
      "\"q\":1},\"nulls\":3}}},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("[{\"p\":\"q\"},{},{},{}]")));
}

TEST(InferJSONSchema, SingleExplicitNull) {
  EXPECT_EQ("{\"ObjectOrArray\":{\"fields\":{\"a\":{\"Null\":{\"occurrences\":1}}},\"nulls\":0}}",
            JSON<JSONFormat::Minimalistic>(current::utils::InferRawSchemaFromJSON("{\"a\":null}")));
}

TEST(InferJSONSchema, SeveralExplicitNulls) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{\"a\":{\"Null\":{\"occurrences\":4}"
      "},\"b\":{\"Null\":{\"occurrences\":4}}},\"nulls\":0}}},\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(
          current::utils::InferRawSchemaFromJSON("[{\"a\":null,\"b\":null},{\"a\":null},{\"b\":null},{}]")));
}

TEST(InferJSONSchema, MissingObject) {
  EXPECT_EQ(
      "{\"ObjectOrArray\":{\"fields\":{\"\":{\"ObjectOrArray\":{\"fields\":{\"x\":{\"ObjectOrArray\":{"
      "\"fields\":{\"a\":{\"String\":{\"values\":{\"b\":1},\"nulls\":0}}},\"nulls\":1}}},\"nulls\":0}}},"
      "\"nulls\":0}}",
      JSON<JSONFormat::Minimalistic>(
          current::utils::InferRawSchemaFromJSON("[{\"x\":{\"a\":\"b\"}},{\"x\":null}]")));
}

// Real `CURRENT_STRUCT` tests.
TEST(InferJSONSchema, Blah) {
  //  EXPECT_EQ("blah",
  //        current::utils::InferSchemaFromJSON("[{\"x\":\"a\",\"z\":\"\"},{\"y\":\"b\",\"z\":\"\"}]"));
}

// RapidJSON usage snippets framed as unit tests. Let's keep them in this `test.cc`. -- D.K.
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
