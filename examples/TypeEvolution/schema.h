/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef SCHEMA_H
#define SCHEMA_H

#include "../../current.h"

#include <type_traits>

// An almost-POD struct with only basic types.
CURRENT_STRUCT(Basic) {
  CURRENT_FIELD(i, int32_t);
  CURRENT_FIELD(s, std::string);
  CURRENT_FIELD(t, std::chrono::microseconds);
  CURRENT_DEFAULT_CONSTRUCTOR(Basic) {}
  CURRENT_CONSTRUCTOR(Basic)(std::integral_constant<int, 0>) : i(42), s("answer"), t(std::chrono::microseconds(123)) {}
};

// A struct with changing fields, that requires evolution.
CURRENT_STRUCT(FullName) {
#ifndef OLD_SCHEMA
  CURRENT_FIELD(full_name, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(FullName) {}
  CURRENT_CONSTRUCTOR(FullName)(std::integral_constant<int, 1>) : full_name("Korolev, Dima") {}
  CURRENT_CONSTRUCTOR(FullName)(std::integral_constant<int, 2>) : full_name("Zhurovich, Max") {}
#else
  CURRENT_FIELD(first_name, std::string);
  CURRENT_FIELD(last_name, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(FullName) {}
  CURRENT_CONSTRUCTOR(FullName)(std::integral_constant<int, 1>) : first_name("Dima"), last_name("Korolev") {}
  CURRENT_CONSTRUCTOR(FullName)(std::integral_constant<int, 2>) : first_name("Max"), last_name("Zhurovich") {}
#endif
};

CURRENT_STRUCT(WithOptional) {
  CURRENT_FIELD(maybe_name, Optional<FullName>);
  CURRENT_DEFAULT_CONSTRUCTOR(WithOptional) {}
  CURRENT_CONSTRUCTOR(WithOptional)(std::integral_constant<int, 2>) : maybe_name(nullptr) {}
  CURRENT_CONSTRUCTOR(WithOptional)
  (std::integral_constant<int, 3>) : maybe_name(FullName(std::integral_constant<int, 2>())) {}
};

CURRENT_STRUCT(CustomTypeA) {
  CURRENT_FIELD(a, int32_t);
  CURRENT_CONSTRUCTOR(CustomTypeA)(int32_t a = 1) : a(a) {}
};

CURRENT_STRUCT(CustomTypeB) {
  CURRENT_FIELD(b, int32_t);
  CURRENT_CONSTRUCTOR(CustomTypeB)(int32_t b = 2) : b(b) {}
};

CURRENT_STRUCT(CustomTypeC) {
  CURRENT_FIELD(c, int32_t);
  CURRENT_CONSTRUCTOR(CustomTypeC)(int32_t c = 3) : c(c) {}
};

#ifndef OLD_SCHEMA
CURRENT_VARIANT(ExpandingVariant, CustomTypeA, CustomTypeB, CustomTypeC);
CURRENT_VARIANT(ShrinkingVariant, CustomTypeA, CustomTypeB);
#else
CURRENT_VARIANT(ExpandingVariant, CustomTypeA, CustomTypeB);
CURRENT_VARIANT(ShrinkingVariant, CustomTypeA, CustomTypeB, CustomTypeC);
#endif

CURRENT_STRUCT(WithExpandingVariant) {
  CURRENT_FIELD(v, ExpandingVariant);
  CURRENT_DEFAULT_CONSTRUCTOR(WithExpandingVariant) {}
  CURRENT_CONSTRUCTOR(WithExpandingVariant)(std::integral_constant<int, 4>) : v(CustomTypeA()) {}
};

CURRENT_STRUCT(WithShrinkingVariant) {
  CURRENT_FIELD(v, ShrinkingVariant);
  CURRENT_DEFAULT_CONSTRUCTOR(WithShrinkingVariant) {}
#ifndef OLD_SCHEMA
  // The evolver would add one, so `C.c == 10` would become `A.a == 11`.
  CURRENT_CONSTRUCTOR(WithShrinkingVariant)(std::integral_constant<int, 5>) : v(CustomTypeA(11)) {}
#else
  CURRENT_CONSTRUCTOR(WithShrinkingVariant)(std::integral_constant<int, 5>) : v(CustomTypeC(10)) {}
#endif
};

CURRENT_STRUCT(WithFieldsToRemove) {
  CURRENT_FIELD(foo, std::string, "thug");
  CURRENT_FIELD(bar, std::string, "life");
#ifndef OLD_SCHEMA
  CURRENT_DEFAULT_CONSTRUCTOR(WithFieldsToRemove) { foo = "thug not so much yoda says"; }
#else
  CURRENT_FIELD(baz, std::vector<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(WithFieldsToRemove) { baz = {"not", "so", "much", "yoda", "says"}; }
#endif
};

CURRENT_VARIANT(All, Basic, FullName, WithOptional, WithExpandingVariant, WithShrinkingVariant, WithFieldsToRemove);

CURRENT_STRUCT(TopLevel) {
  CURRENT_FIELD(data, All);
  CURRENT_CONSTRUCTOR(TopLevel)(std::integral_constant<int, 0>) : data(Basic(std::integral_constant<int, 0>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)(std::integral_constant<int, 1>) : data(FullName(std::integral_constant<int, 1>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)
  (std::integral_constant<int, 2>) : data(WithOptional(std::integral_constant<int, 2>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)
  (std::integral_constant<int, 3>) : data(WithOptional(std::integral_constant<int, 3>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)
  (std::integral_constant<int, 4>) : data(WithExpandingVariant(std::integral_constant<int, 4>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)
  (std::integral_constant<int, 5>) : data(WithShrinkingVariant(std::integral_constant<int, 5>())) {}
  CURRENT_CONSTRUCTOR(TopLevel)(std::integral_constant<int, 6>) : data(WithFieldsToRemove()) {}
};

#endif  // SCHEMA_H
