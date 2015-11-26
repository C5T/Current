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

#ifndef BRICKS_CEREALIZE_DOCU_01CEREALIZE_04_TEST_CC
#define BRICKS_CEREALIZE_DOCU_01CEREALIZE_04_TEST_CC

#include "../cerealize.h"
#include "../../strings/printf.h"

#include "../../../3rdparty/gtest/gtest-main.h"

using namespace current;
using strings::Printf;

// Should keep the indent for docu autogeneration.
#if 0
  // The example below uses `Printf()`, include it.
  #include "strings/printf.h"
  using current::strings::Printf;
   
#endif

  // Polymorphic types are supported with some caution.
  struct ExamplePolymorphicType {
    std::string base;

    explicit ExamplePolymorphicType(const std::string& base = "") : base(base) {}
  
    virtual std::string AsString() const = 0;

    template <typename A> void serialize(A& ar) {
      ar(CEREAL_NVP(base));
    }
  };
  
  struct ExamplePolymorphicInt : ExamplePolymorphicType {
    int i;

    explicit ExamplePolymorphicInt(int i = 0)
        : ExamplePolymorphicType("int"), i(i) {}
  
    virtual std::string AsString() const override {
      return Printf("%s, %d", base.c_str(), i);
    }
  
    template <typename A> void serialize(A& ar) {
      ExamplePolymorphicType::serialize(ar);
      ar(CEREAL_NVP(i));
    }
  };
  // Need to register the derived type.
  CEREAL_REGISTER_TYPE(ExamplePolymorphicInt);
  
  struct ExamplePolymorphicDouble : ExamplePolymorphicType {
    double d;

    explicit ExamplePolymorphicDouble(double d = 0)
        : ExamplePolymorphicType("double"), d(d) {}
  
    virtual std::string AsString() const override {
      return Printf("%s, %lf", base.c_str(), d);
    }
  
    template <typename A> void serialize(A& ar) {
      ExamplePolymorphicType::serialize(ar);
      ar(CEREAL_NVP(d));
    }
  };
  // Need to register the derived type.
  CEREAL_REGISTER_TYPE(ExamplePolymorphicDouble);
  
TEST(Docu, Cereal04) {
  const std::string json_int =
    CerealizeJSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicInt(42)));
  
  const std::string json_double =
    CerealizeJSON(WithBaseType<ExamplePolymorphicType>(ExamplePolymorphicDouble(M_PI)));
  
  EXPECT_EQ("int, 42",
            CerealizeParseJSON<std::unique_ptr<ExamplePolymorphicType>>(json_int)->AsString());
  
  EXPECT_EQ("double, 3.141593",
            CerealizeParseJSON<std::unique_ptr<ExamplePolymorphicType>>(json_double)->AsString());
}

#endif  // BRICKS_CEREALIZE_DOCU_01CEREALIZE_04_TEST_CC
