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

// The format of the JSON used to export schema information to third parties.

#ifndef CURRENT_TYPE_SYSTEM_SCHEMA_JSON_SCHEMA_FORMAT_CC
#define CURRENT_TYPE_SYSTEM_SCHEMA_JSON_SCHEMA_FORMAT_CC

#include "../Reflection/types.h"  // TypeID.
#include "../struct.h"

namespace current {
namespace reflection {

namespace variant_clean_type_names {

// clang-format off

CURRENT_FORWARD_DECLARE_STRUCT(primitive);
CURRENT_FORWARD_DECLARE_STRUCT(key);                   // a.k.a. `CURRENT_ENUM()`.
CURRENT_FORWARD_DECLARE_STRUCT(array);                 // a.k.a. `vector<>`.
CURRENT_FORWARD_DECLARE_STRUCT(ordered_dictionary);    // a.k.a. `map<>`.
CURRENT_FORWARD_DECLARE_STRUCT(unordered_dictionary);  // a.k.a. `unordered_map<>`.
CURRENT_FORWARD_DECLARE_STRUCT(ordered_set);           // a.k.a. `set<>`.
CURRENT_FORWARD_DECLARE_STRUCT(unordered_set);         // a.k.a. `unordered_set<>`.
CURRENT_FORWARD_DECLARE_STRUCT(pair);                  // a.k.a. `std::pair<>`.
CURRENT_FORWARD_DECLARE_STRUCT(optional);              // a.k.a. `Optional<>`.
CURRENT_FORWARD_DECLARE_STRUCT(algebraic);             // a.k.a. `Variant<>.
CURRENT_FORWARD_DECLARE_STRUCT(object);                // a.k.a. `CURRENT_STRUCT()`.
CURRENT_FORWARD_DECLARE_STRUCT(error);

using type_variant_t = Variant<primitive, key, array, ordered_dictionary, unordered_dictionary, ordered_set, unordered_set, pair, optional, algebraic, object, error>;

CURRENT_STRUCT(primitive) {
  CURRENT_FIELD(type, std::string);
  CURRENT_FIELD(text, std::string);
};

CURRENT_STRUCT(key) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(type, std::string);
  CURRENT_FIELD(text, std::string);
};

CURRENT_STRUCT(array) {
  CURRENT_FIELD(element, type_variant_t);
};

CURRENT_STRUCT(ordered_dictionary) {
  CURRENT_FIELD(from, type_variant_t);
  CURRENT_FIELD(into, type_variant_t);
};

CURRENT_STRUCT(unordered_dictionary) {
  CURRENT_FIELD(from, type_variant_t);
  CURRENT_FIELD(into, type_variant_t);
};

CURRENT_STRUCT(ordered_set) {
  CURRENT_FIELD(of, type_variant_t);
};

CURRENT_STRUCT(unordered_set) {
  CURRENT_FIELD(of, type_variant_t);
};

CURRENT_STRUCT(pair) {
  CURRENT_FIELD(first, type_variant_t);
  CURRENT_FIELD(second, type_variant_t);
};

CURRENT_STRUCT(optional) {
  CURRENT_FIELD(underlying, type_variant_t);
};

CURRENT_STRUCT(algebraic) {
  CURRENT_FIELD(cases, std::vector<type_variant_t>);
};

CURRENT_STRUCT(object) {
  CURRENT_FIELD(kind, std::string);
};

CURRENT_STRUCT(error) {
  CURRENT_FIELD(error, std::string);
  CURRENT_FIELD(internal, TypeID);
};

// clang-format on

}  // namespace current::reflection::variant_clean_type_names

CURRENT_STRUCT(JSONSchemaObjectField) {
  CURRENT_FIELD(field, std::string);
  CURRENT_FIELD(description, Optional<std::string>);
  CURRENT_FIELD(as, variant_clean_type_names::type_variant_t);
};

CURRENT_STRUCT(JSONSchemaObject) {
  CURRENT_FIELD(object, std::string);
  CURRENT_FIELD(contains, std::vector<JSONSchemaObjectField>);
};

using JSONSchema = std::vector<JSONSchemaObject>;

}  // namespace current::reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SCHEMA_JSON_SCHEMA_FORMAT_CC
