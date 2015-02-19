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

#ifndef BRICKS_CEREALIZE_DOCU_01CEREALIZE_02_TEST_CC
#define BRICKS_CEREALIZE_DOCU_01CEREALIZE_02_TEST_CC

#include "docu_01cerealize_01_test.cc"

using namespace bricks;
using namespace cerealize;

using docu::SimpleType;

TEST(Docu, Cereal02) {
  // Use `JSON()` and `JSONParse()` to create and parse JSON-s.
  SimpleType x;
  x.number = 42;
  x.string = "test passed";
  x.vector_int.push_back(1);
  x.vector_int.push_back(2);
  x.vector_int.push_back(3);
  x.map_int_string[1] = "one";
  x.map_int_string[42] = "the question";
  
  // `JSON(object)` converts a cerealize-able object into a JSON string.
  const std::string json = JSON(x);
EXPECT_EQ("{\"value0\":{"
"\"number\":42,\"string\":\"test passed\","
"\"vector_int\":[1,2,3],"
"\"map_int_string\":[{\"key\":1,\"value\":\"one\"},{\"key\":42,\"value\":\"the question\"}]}"
"}", json);
  
  // `JSONParse<T>(json)` creates an instance of T from a JSON.
  const SimpleType y = JSONParse<SimpleType>(json);
EXPECT_EQ(42, y.number);
EXPECT_EQ("test passed", y.string);
EXPECT_EQ(3u, y.vector_int.size());
EXPECT_EQ(2u, y.map_int_string.size());
  
  // `JSONParse(json, T& out)` allows omitting the type.
  SimpleType z;
  JSONParse(json, z);
EXPECT_EQ(42, z.number);
EXPECT_EQ("test passed", z.string);
EXPECT_EQ(3u, z.vector_int.size());
EXPECT_EQ(2u, z.map_int_string.size());
}

#endif  // BRICKS_CEREALIZE_DOCU_01CEREALIZE_02_TEST_CC
