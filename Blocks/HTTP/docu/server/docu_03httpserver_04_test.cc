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

#ifndef BLOCKS_HTTP_DOCU_SERVER_04_TEST_CC
#define BLOCKS_HTTP_DOCU_SERVER_04_TEST_CC

#include <algorithm>
#include <functional>

#include "../../api.h"
#include "../../../../Bricks/strings/printf.h"
#include "../../../../Bricks/dflags/dflags.h"
#include "../../../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_int32(docu_net_server_port_04, PickPortForUnitTest(), "");

using current::strings::Printf;

  // An input record that would be passed in as a JSON.
  CURRENT_STRUCT(PennyInput) {
    CURRENT_FIELD(op, std::string);
    CURRENT_FIELD(x, std::vector<int32_t>);
    CURRENT_DEFAULT_CONSTRUCTOR(PennyInput) {}
    CURRENT_CONSTRUCTOR(PennyInput)(const std::string& op, const std::vector<int32_t>& x) : op(op), x(x) {}
  };
  
  // An output record that would be sent back as a JSON.
  CURRENT_STRUCT(PennyOutput) {
    CURRENT_FIELD(error, std::string);
    CURRENT_FIELD(result, int32_t);
    CURRENT_CONSTRUCTOR(PennyOutput)(const std::string& error, int32_t result) : error(error), result(result) {}
  }; 
  
TEST(Docu, HTTPServer04) {
const auto port = FLAGS_docu_net_server_port_04;
  // Doing Penny-level arithmetics for fun and performance testing.
  const auto scope = HTTP(port).Register("/penny", [](Request r) {
    try {
      const auto input = ParseJSON<PennyInput>(r.body);
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
    } catch (const current::Exception& e) {
      // TODO(dkorolev): Catch the right exception type.
      r(PennyOutput{e.What(), 0});
    }
  });
EXPECT_EQ("{\"error\":\"\",\"result\":5}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), PennyInput{"add",{2,3}})).body);
EXPECT_EQ("{\"error\":\"\",\"result\":6}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), PennyInput{"mul",{2,3}})).body);
EXPECT_EQ("{\"error\":\"Unknown operation: sqrt\",\"result\":0}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), PennyInput{"sqrt",{}})).body);
EXPECT_EQ("{\"error\":\"Not enough arguments for 'add'.\",\"result\":0}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), PennyInput{"add",{}})).body);
EXPECT_EQ("{\"error\":\"Not enough arguments for 'mul'.\",\"result\":0}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), PennyInput{"mul",{}})).body);
// TODO(dkorolev): Make sure to test the proper, complete, error message.
EXPECT_EQ("{\"error\":\"FFFUUUUU\",\"result\":0}\n", HTTP(POST(Printf("http://localhost:%d/penny", port), "FFFUUUUU", "text/plain")).body);
}

#endif  // BLOCKS_HTTP_DOCU_SERVER_04_TEST_CC
