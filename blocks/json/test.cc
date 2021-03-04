/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2020 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

TEST(UniversalJSON, Primitives) {
  using namespace current::json;

  EXPECT_TRUE(Exists<JSONString>(ParseJSONUniversally("\"foo\"")));
  EXPECT_EQ("\"foo\"", AsJSON(ParseJSONUniversally("\"foo\"")));

  EXPECT_TRUE(Exists<JSONNumber>(ParseJSONUniversally("42")));
  EXPECT_EQ("42", AsJSON(ParseJSONUniversally("42")));

  EXPECT_TRUE(Exists<JSONBoolean>(ParseJSONUniversally("true")));
  EXPECT_EQ("true", AsJSON(ParseJSONUniversally("true")));

  EXPECT_TRUE(Exists<JSONNull>(ParseJSONUniversally("null")));
  EXPECT_EQ("null", AsJSON(ParseJSONUniversally("null")));

  EXPECT_TRUE(Exists<JSONArray>(ParseJSONUniversally("[]")));
  EXPECT_EQ("[]", AsJSON(ParseJSONUniversally("[]")));

  EXPECT_TRUE(Exists<JSONObject>(ParseJSONUniversally("{}")));
  EXPECT_EQ("{}", AsJSON(ParseJSONUniversally("{}")));
}

TEST(UniversalJSON, Numbers) {
  using namespace current::json;

  EXPECT_EQ("0", AsJSON(ParseJSONUniversally("0")));
  EXPECT_EQ("0.5", AsJSON(ParseJSONUniversally("0.5")));
  EXPECT_EQ("-0.15", AsJSON(ParseJSONUniversally("-0.15")));
  EXPECT_EQ("0.12345", AsJSON(ParseJSONUniversally("0.12345")));
  EXPECT_EQ("-0.98765", AsJSON(ParseJSONUniversally("-0.98765")));
  EXPECT_EQ("-12345", AsJSON(ParseJSONUniversally("-12345")));
  EXPECT_EQ("98765", AsJSON(ParseJSONUniversally("98765")));
  EXPECT_EQ("1000000000000000", AsJSON(ParseJSONUniversally("1e+15")));
  EXPECT_EQ("1e-15", AsJSON(ParseJSONUniversally("1e-15")));
}

TEST(UniversalJSON, Array) {
  using namespace current::json;

  const std::string json = "[1,\"bar\",null,true,[],{}]";
  const JSONValue parsed = ParseJSONUniversally(json);
  EXPECT_TRUE(Exists<JSONArray>(parsed));
  EXPECT_EQ(json, AsJSON(parsed));

  ASSERT_EQ(6u, Value<JSONArray>(parsed).size());
  std::vector<std::string> results;
  for (const JSONValue& value : Value<JSONArray>(parsed)) {
    results.push_back(AsJSON(value));
  }
  ASSERT_EQ(6u, results.size());
  EXPECT_EQ("1", results[0]);
  EXPECT_EQ("\"bar\"", results[1]);
  EXPECT_EQ("null", results[2]);
  EXPECT_EQ("true", results[3]);
  EXPECT_EQ("[]", results[4]);
  EXPECT_EQ("{}", results[5]);
}

TEST(UniversalJSON, Object) {
  using namespace current::json;

  const std::string json = "{\"a\":101,\"c\":\"the order is preserved\",\"b\":null}";
  const JSONValue parsed = ParseJSONUniversally(json);
  EXPECT_TRUE(Exists<JSONObject>(parsed));
  EXPECT_EQ(json, AsJSON(parsed));

  ASSERT_EQ(3u, Value<JSONObject>(parsed).size());
  std::vector<std::string> results;
  for (const JSONObject::const_element element : Value<JSONObject>(parsed)) {
    results.push_back(element.key + " : " + AsJSON(element.value));
  }
  ASSERT_EQ(3u, results.size());
  EXPECT_EQ("a : 101", results[0]);
  EXPECT_EQ("c : \"the order is preserved\"", results[1]);
  EXPECT_EQ("b : null", results[2]);

  JSONValue mutable_parsed = parsed;
  EXPECT_EQ(json, AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).erase("z");
  EXPECT_EQ(json, AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).erase("a");
  EXPECT_EQ("{\"c\":\"the order is preserved\",\"b\":null}", AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).push_back("a", JSONNumber(101));
  EXPECT_EQ("{\"c\":\"the order is preserved\",\"b\":null,\"a\":101}", AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).erase("b").erase("c").push_back("c", JSONString("foo")).push_back("b", JSONNull());
  EXPECT_EQ("{\"a\":101,\"c\":\"foo\",\"b\":null}", AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).push_back("c", JSONString("bar"));
  EXPECT_EQ("{\"a\":101,\"b\":null,\"c\":\"bar\"}", AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).push_back("a", JSONNumber(888));
  EXPECT_EQ("{\"b\":null,\"c\":\"bar\",\"a\":888}", AsJSON(mutable_parsed));
  Value<JSONObject>(mutable_parsed).push_back("a", JSONNumber(999));
  EXPECT_EQ("{\"b\":null,\"c\":\"bar\",\"a\":999}", AsJSON(mutable_parsed));

  using JO = JSONObject;
  using JN = JSONNumber;
  EXPECT_EQ(
      "{\"x\":42,\"y\":{\"a\":1,\"b\":2}}",
      AsJSON(JO().push_back("x", JN(42)).push_back("y", JO().push_back("a", JN(1)).push_back("b", JN(2)))));
}

TEST(UniversalJSON, FloatingPoint) {
  using namespace current::json;

  {
    const std::string json = "{\"x\":1234.56}";
    EXPECT_EQ(json, AsJSON(ParseJSONUniversally(json)));
  }
  {
    const std::string json = "{\"y\":12345.67}";
    EXPECT_EQ(json, AsJSON(ParseJSONUniversally(json)));
  }
  {
    const std::string json = "{\"z\":9876543210}";
    EXPECT_EQ(json, AsJSON(ParseJSONUniversally(json)));
  }
}
