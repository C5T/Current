/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "struct.h"

#include "../3rdparty/gtest/gtest-main.h"

namespace struct_definition_test {

// A few properly defined Current data types.
CURRENT_STRUCT(Foo) { CURRENT_FIELD(i, uint64_t, 42u); };
CURRENT_STRUCT(Bar) {
  CURRENT_FIELD(v1, std::vector<uint64_t>);
  CURRENT_FIELD(v2, std::vector<Foo>);
  CURRENT_FIELD(v3, std::vector<std::vector<Foo>>);
};
CURRENT_STRUCT(DerivedFromFoo, Foo) { CURRENT_FIELD(bar, Bar); };

static_assert(IS_VALID_CURRENT_STRUCT(Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(Bar), "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(DerivedFromFoo), "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace struct_definition_test

// Confirm that Current data types defined in a namespace are accessible from outside it.
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Foo), "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::Bar), "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

namespace some_other_namespace {

// Confirm that Current data types defined in one namespace are accessible from another one.
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Foo),
              "Struct `Foo` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::Bar),
              "Struct `Bar` was not properly declared.");
static_assert(IS_VALID_CURRENT_STRUCT(::struct_definition_test::DerivedFromFoo),
              "Struct `DerivedFromFoo` was not properly declared.");

}  // namespace some_other_namespace

namespace struct_definition_test {

// Properly declared structures.
CURRENT_STRUCT(Empty){};
CURRENT_STRUCT(EmptyDerived, Empty){};

static_assert(IS_VALID_CURRENT_STRUCT(Empty), "`Empty` must pass `IS_VALID_CURRENT_STRUCT` check.");
static_assert(IS_VALID_CURRENT_STRUCT(EmptyDerived),
              "`EmptyDerived` must pass `IS_VALID_CURRENT_STRUCT` check.");

// Improperly declared structures.
struct WrongStructNotCurrentStruct {
  int x;
};
struct WrongDerivedStructNotCurrentStruct : ::current::reflection::CurrentSuper {};
struct NotCurrentStructDerivedFromCurrentStruct : Empty {};

CURRENT_STRUCT(WrongUsesCOUNTERInternally) {
  CURRENT_FIELD(i1, uint64_t);
  static size_t GetCounter() { return __COUNTER__; }
  CURRENT_FIELD(i2, uint64_t);
};

// The lines below don't compile with various errors.
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongStructNotCurrentStruct),
//               "`WrongStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongDerivedStructNotCurrentStruct),
//               "`WrongDerivedStructNotCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(NotCurrentStructDerivedFromCurrentStruct),
//               "`NotCurrentStructDerivedFromCurrentStruct` must NOT pass `IS_VALID_CURRENT_STRUCT` check.");
// static_assert(!IS_VALID_CURRENT_STRUCT(WrongUsesCOUNTERInternally),
//               "`WrongUsesCOUNTERInternally` must not pass `IS_VALID_CURRENT_STRUCT` check.");

}  // namespace struct_definition_test
