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

#ifndef BRICKS_CEREALIZE_DOCU_01CEREALIZE_03_TEST_CC
#define BRICKS_CEREALIZE_DOCU_01CEREALIZE_03_TEST_CC

#include "../cerealize.h"

#include "../../3party/gtest/gtest-main.h"

using namespace bricks;
using namespace cerealize;

namespace docu {  // Should keep the indent for docu autogeneration.
  // Use `load()/save()` instead of `serialize()` to customize serialization.
  struct LoadSaveType {
    int a;
    int b;
    int sum;
    
    template <typename A> void save(A& ar) const {
      ar(CEREAL_NVP(a), CEREAL_NVP(b));
    }
  
    template <typename A> void load(A& ar) {
      ar(CEREAL_NVP(a), CEREAL_NVP(b));
      sum = a + b;
    }
  };
  
}  // namespace docu

using docu::LoadSaveType;

TEST(CerealDocu, Docu03) {
  LoadSaveType x;
  x.a = 2;
  x.b = 3;
  EXPECT_EQ(5, JSONParse<LoadSaveType>(JSON(x)).sum);
}

#endif  // BRICKS_CEREALIZE_DOCU_01CEREALIZE_03_TEST_CC
