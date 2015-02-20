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

#ifndef BRICKS_NET_API_DOCU_SERVER_04_TEST_CC
#define BRICKS_NET_API_DOCU_SERVER_04_TEST_CC

#include <algorithm>
#include <functional>

#include "../../api.h"
#include "../../../../strings/printf.h"
#include "../../../../dflags/dflags.h"
#include "../../../../3party/gtest/gtest-main-with-dflags.h"

DEFINE_int32(docu_net_server_port_04, 8082, "Okay to keep the same as in net/api/test.cc");

using namespace bricks::net::api;
using bricks::strings::Printf;
using bricks::net::HTTPHeaders;

  // An input record that would be passed in as a JSON.
  struct PennyInput {
    std::string op;
    std::vector<int> x;
    std::string error;  // Not serialized.
    template <typename A> void serialize(A& ar) {
      ar(CEREAL_NVP(op), CEREAL_NVP(x));
    }
    void FromInvalidJSON(const std::string& input_json) {
      error = "JSON parse error: " + input_json;
    }
  };
  
  // An output record that would be sent back as a JSON.
  struct PennyOutput {
    std::string error;
    int result;
    template <typename A> void serialize(A& ar) {
      ar(CEREAL_NVP(error), CEREAL_NVP(result));
    }
  }; 
  
TEST(Docu, HTTPServer04) {
const auto port = FLAGS_docu_net_server_port_04;
HTTP(port).ResetAllHandlers();
  // Doing Penny-level arithmetics for fun and performance testing.
  HTTP(port).Register("/penny", [](Request r) {
    const auto input = JSONParse<PennyInput>(r.body);
    if (!input.error.empty()) {
      r(PennyOutput{input.error, 0});
    } else {
      if (input.op == "add") {
        if (!input.x.empty()) {
          int result = 0;
          for (const auto v : input.x) {
            result += v;
          }
          r(PennyOutput{"", result});
        } else {
          r(PennyOutput{"Not enough arguments for 'add'.", 0});
        }
      } else if (input.op == "mul") {
        if (!input.x.empty()) {
          int result = 1;
          for (const auto v : input.x) {
            result *= v;
          }
          r(PennyOutput{"", result});
        } else {
          r(PennyOutput{"Not enough arguments for 'mul'.", 0});
        }
      } else {
        r(PennyOutput{"Unknown operation: " + input.op, 0});
      }
    }
  });
EXPECT_EQ("{\"value0\":{\"error\":\"\",\"result\":5}}\n", HTTP(POST(Printf("localhost:%d/penny", port), PennyInput{"add",{2,3},""})).body);
EXPECT_EQ("{\"value0\":{\"error\":\"\",\"result\":6}}\n", HTTP(POST(Printf("localhost:%d/penny", port), PennyInput{"mul",{2,3},""})).body);
EXPECT_EQ("{\"value0\":{\"error\":\"Unknown operation: sqrt\",\"result\":0}}\n", HTTP(POST(Printf("localhost:%d/penny", port), PennyInput{"sqrt",{},""})).body);
EXPECT_EQ("{\"value0\":{\"error\":\"Not enough arguments for 'add'.\",\"result\":0}}\n", HTTP(POST(Printf("localhost:%d/penny", port), PennyInput{"add",{},""})).body);
EXPECT_EQ("{\"value0\":{\"error\":\"Not enough arguments for 'mul'.\",\"result\":0}}\n", HTTP(POST(Printf("localhost:%d/penny", port), PennyInput{"mul",{},""})).body);
EXPECT_EQ("{\"value0\":{\"error\":\"JSON parse error: FFFUUUUU\",\"result\":0}}\n", HTTP(POST(Printf("localhost:%d/penny", port), "FFFUUUUU", "text/plain")).body);
}

#endif  // BRICKS_NET_API_DOCU_SERVER_04_TEST_CC
