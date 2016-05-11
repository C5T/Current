/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_EVOLUTION_TEST_CC
#define CURRENT_TYPE_SYSTEM_EVOLUTION_TEST_CC

#include "type_evolution.h"

#include "../struct.h"
#include "../variant.h"

#include "../Serialization/json.h"

#include "../../3rdparty/gtest/gtest-main.h"

namespace type_evolution_test {

namespace from {
CURRENT_STRUCT(SimpleStruct) {
  CURRENT_FIELD(x, int32_t, 101);
  CURRENT_FIELD(y, int32_t, 102);
};
}  // namespace type_evolution_test::from

namespace into {
CURRENT_STRUCT(SimpleStruct) {
  CURRENT_FIELD(x, int32_t, 201);
  CURRENT_FIELD(y, int32_t, 202);
};
}  // namespace type_evolution_test::into

}  // namespace type_evolution_test

namespace type_evolution {

template <typename INTO>
struct Evolve<type_evolution_test::from::SimpleStruct, INTO> {
  using from_t = type_evolution_test::from::SimpleStruct;
  using into_t = INTO;
  static_assert(IS_CURRENT_STRUCT(into_t), "Must evolve a `CURRENT_STRUCT` into a `CURRENT_STRUCT`.");
  static_assert(current::reflection::FieldCounter<into_t>::value == 2,
                "Must evolve into a `CURRENT_STRUCT` with the same number of fields.");
  static into_t Go(const from_t& from) {
    into_t into;
    into.x = Evolve<decltype(from.x), decltype(into.x)>::Go(from.x);
    into.y = Evolve<decltype(from.y), decltype(into.y)>::Go(from.y);
    return into;
  }
};

}  // namespace ::type_evolution

TEST(TypeEvolutionTest, SimpleStruct) {
  using namespace type_evolution_test;

  from::SimpleStruct from;
  EXPECT_EQ("{\"x\":101,\"y\":102}", JSON(from));

  {
    into::SimpleStruct into;
    EXPECT_EQ("{\"x\":201,\"y\":202}", JSON(into));
  }

  {
    into::SimpleStruct converted = type_evolution::Evolve<from::SimpleStruct, into::SimpleStruct>::Go(from);
    EXPECT_EQ("{\"x\":101,\"y\":102}", JSON(converted));
  }
}

namespace type_evolution_test {

namespace from {
CURRENT_STRUCT(StructWithStruct) { CURRENT_FIELD(s, SimpleStruct); };
}  // namespace type_evolution_test::from

namespace into {
CURRENT_STRUCT(StructWithStruct) { CURRENT_FIELD(s, SimpleStruct); };
}  // namespace type_evolution_test::into

}  // namespace type_evolution_test

namespace type_evolution {

template <typename INTO>
struct Evolve<type_evolution_test::from::StructWithStruct, INTO> {
  using from_t = type_evolution_test::from::StructWithStruct;
  using into_t = INTO;
  static_assert(IS_CURRENT_STRUCT(into_t), "Must evolve a `CURRENT_STRUCT` into a `CURRENT_STRUCT`.");
  static_assert(current::reflection::FieldCounter<into_t>::value == 1,
                "Must evolve into a `CURRENT_STRUCT` with the same number of fields.");
  static into_t Go(const from_t& from) {
    into_t into;
    into.s = Evolve<decltype(from.s), decltype(into.s)>::Go(from.s);
    return into;
  }
};

}  // namespace ::type_evolution

TEST(TypeEvolutionTest, StructWithStruct) {
  using namespace type_evolution_test;

  from::StructWithStruct from;
  EXPECT_EQ("{\"s\":{\"x\":101,\"y\":102}}", JSON(from));

  {
    into::StructWithStruct into;
    EXPECT_EQ("{\"s\":{\"x\":201,\"y\":202}}", JSON(into));
  }

  {
    into::StructWithStruct converted =
        type_evolution::Evolve<from::StructWithStruct, into::StructWithStruct>::Go(from);
    EXPECT_EQ("{\"s\":{\"x\":101,\"y\":102}}", JSON(converted));
  }
}

#endif  // CURRENT_TYPE_SYSTEM_EVOLUTION_TEST_CC
