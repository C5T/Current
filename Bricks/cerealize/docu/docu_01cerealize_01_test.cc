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

#ifndef BRICKS_CEREALIZE_DOCU_01CEREALIZE_01_TEST_CC
#define BRICKS_CEREALIZE_DOCU_01CEREALIZE_01_TEST_CC

#include <vector>
#include <map>

#include "../cerealize.h"

#include "../../3party/gtest/gtest-main.h"

using namespace bricks;

namespace docu {  // Should keep the indent for docu autogeneration.
  // Add a `serialize()` method to make a C++ structure "cerealizable".
  struct SimpleType {
    int number;
    std::string string;
    std::vector<int> vector_int;
    std::map<int, std::string> map_int_string;

    template <typename A> void serialize(A& ar) {
      // Use `CEREAL_NVP(member)` to keep member names when using JSON.
      ar(CEREAL_NVP(number),
         CEREAL_NVP(string),
         CEREAL_NVP(vector_int),
         CEREAL_NVP(map_int_string));
    }
  };
}  // namespace docu

#endif  // BRICKS_CEREALIZE_DOCU_01CEREALIZE_01_TEST_CC
