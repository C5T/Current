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

// Infers the `CURRENT_STRUCT`-based schema from a given JSON.

// TODO(dkorolev): Infer number types: integer/double, signed/unsigned, 32/64-bit.
// TODO(dkorolev): Split into several header files.

#ifndef CURRENT_UTILS_JSONSCHEMA_INFER_H
#define CURRENT_UTILS_JSONSCHEMA_INFER_H

#include "../../TypeSystem/struct.h"
#include "../../TypeSystem/Schema/schema.h"
#include "../../TypeSystem/Serialization/json.h"

namespace current {
namespace utils {

struct InferSchemaException : Exception {
  InferSchemaException() : Exception() {}
  explicit InferSchemaException(const std::string& message) : Exception(message) {}
};

// The input string is not a valid JSON.
struct InferSchemaParseJSONException : InferSchemaException {};

// The input JSON can not be a `CURRENT_STRUCT`.
struct InferSchemaInputException : InferSchemaException {
  InferSchemaInputException() : InferSchemaException() {}
  explicit InferSchemaInputException(const std::string& message) : InferSchemaException(message) {}
};

struct InferSchemaTopLevelEmptyArrayIsNotAllowed : InferSchemaInputException {};
struct InferSchemaArrayOfNullsOrEmptyArraysIsNotAllowed : InferSchemaInputException {};
struct InferSchemaUnsupportedTypeException : InferSchemaInputException {};
struct InferSchemaInvalidCPPIdentifierException : InferSchemaInputException {
  explicit InferSchemaInvalidCPPIdentifierException(const std::string& id)
      : InferSchemaInputException("Invalid C++ identifier used as the key in JSON: '" + id + "'.") {}
};

struct InferSchemaIncompatibleTypesBase : InferSchemaInputException {
  InferSchemaIncompatibleTypesBase() : InferSchemaInputException("Incompatible types.") {}
  explicit InferSchemaIncompatibleTypesBase(const std::string& message) : InferSchemaInputException(message) {}
};

// TODO(dkorolev): Text.
template <typename T_LHS, typename T_RHS>
struct InferSchemaIncompatibleTypes : InferSchemaIncompatibleTypesBase {
  InferSchemaIncompatibleTypes(const T_LHS& lhs, const T_RHS& rhs)
      : InferSchemaIncompatibleTypesBase("Incompatible types: '" + lhs.Type() + "' and '" + rhs.Type() + "'.") {
  }
};

struct InferSchemaArrayAndObjectAreIncompatible : InferSchemaIncompatibleTypesBase {};

namespace impl {

inline bool IsValidCPPIdentifier(const std::string& s) {
  if (s.empty()) {
    return false;
  }
  if (!(s[0] == '_' || std::isalpha(s[0]))) {
    return false;
  }
  for (size_t i = 1u; i < s.length(); ++i) {
    if (!std::isalnum(s[i])) {
      return false;
    }
  }
  return true;
}

inline std::string MaybeOptionalType(bool has_nulls, const std::string& type) {
  return has_nulls ? "Optional<" + type + ">" : type;
}

// Inferred JSON types for fields.
// Internally, the type is maintained along with the hitsogram of its values seen.
CURRENT_STRUCT(String) {
  CURRENT_FIELD(values, (std::map<std::string, size_t>));
  CURRENT_FIELD(instances, size_t, 1);
  CURRENT_FIELD(nulls, size_t, 0);

  CURRENT_DEFAULT_CONSTRUCTOR(String) {}
  CURRENT_CONSTRUCTOR(String)(const std::string& string) { values[string] = 1; }

  std::string Type() const { return MaybeOptionalType(nulls, "std::string"); }
};

CURRENT_STRUCT(Bool) {
  CURRENT_FIELD(values_false, size_t, 0);
  CURRENT_FIELD(values_true, size_t, 0);
  CURRENT_FIELD(nulls, size_t, 0);

  std::string Type() const { return MaybeOptionalType(nulls, "bool"); }
};

// Note: The `Null` type is largely ephemeral. Top-level "null" is still not allowed in input JSONs.
CURRENT_STRUCT(Null) { CURRENT_FIELD(occurrences, size_t, 1); };

// A trick to work around the lack of forward declarations of `CURRENT_STRUCT`-s, and to stay DRY.
CURRENT_STRUCT(ObjectOrArray) {
  CURRENT_FIELD(fields, (std::map<std::string, Variant<String, Bool, Null, ObjectOrArray>>));
  CURRENT_FIELD(instances, size_t, 1);
  CURRENT_FIELD(nulls, size_t, 0);

  // An array is represented as a `ObjectOrArray` with one and only key in `fields` -- the empty string.
  bool IsObject() const { return !fields.count(""); }
  std::string Type() const { return MaybeOptionalType(nulls, IsObject() ? "object" : "array"); }
};

using SchemaMap = decltype(std::declval<ObjectOrArray>().fields);
using Schema = typename SchemaMap::mapped_type;

// `Reduce` and `CallReduce` implements the logic of building the schema for a superset of schemas.
//
// 1) `static Schema Reduce<LHS, RHS>::DoIt(lhs, rhs)` returns the schema containing both `lhs` and `rhs`.
//    Superset construction logic is implemented as template specializations of `Reduce<LHS, RHS>`,
//    with the default implementaiton throwing an exception if `lhs` and `rhs` can not be united
//    under a single `CURRENT_STRUCT`.
//
// 2) `CallReduce(const Schema& lhs, const Schema& rhs)` calls the above `Reduce<LHS, RHS>`
//     for the right underlying types of `lhs` and `rhs` respectively.

template <typename T_LHS, typename T_RHS>
struct Reduce {
  static Schema DoIt(const T_LHS& lhs, const T_RHS& rhs) {
    using InferSchemaIncompatibleTypesException = InferSchemaIncompatibleTypes<T_LHS, T_RHS>;
    CURRENT_THROW(InferSchemaIncompatibleTypesException(lhs, rhs));
  }
};

template <typename T_LHS>
struct RHSExpander {
  const T_LHS& lhs;
  Schema& result;
  RHSExpander(const T_LHS& lhs, Schema& result) : lhs(lhs), result(result) {}

  template <typename T_RHS>
  void operator()(const T_RHS& rhs) {
    result = Reduce<T_LHS, T_RHS>::DoIt(lhs, rhs);
  }
};

struct LHSExpander {
  const Schema& rhs;
  Schema& result;
  LHSExpander(const Schema& rhs, Schema& result) : rhs(rhs), result(result) {}

  template <typename T_LHS>
  void operator()(const T_LHS& lhs) const {
    rhs.Call(RHSExpander<T_LHS>(lhs, result));
  }
};

inline void CallReduce(const Schema& lhs, const Schema& rhs, Schema& result) {
  lhs.Call(LHSExpander(rhs, result));
}

template <>
struct Reduce<Null, Null> {
  static Schema DoIt(const Null& lhs, const Null& rhs) {
    Null result;
    result.occurrences = lhs.occurrences + rhs.occurrences;
    return result;
  }
};

template <typename T>
struct Reduce<T, Null> {
  static Schema DoIt(const T& data, const Null& nulls) {
    T result(data);
    result.nulls += nulls.occurrences;
    return result;
  }
};

template <typename T>
struct Reduce<Null, T> {
  static Schema DoIt(const Null& nulls, const T& data) {
    T result(data);
    result.nulls += nulls.occurrences;
    return result;
  }
};

template <>
struct Reduce<String, String> {
  static Schema DoIt(const String& lhs, const String& rhs) {
    String result(lhs);
    for (const auto& counter : rhs.values) {
      result.values[counter.first] += counter.second;
    }
    result.instances += rhs.instances;
    result.nulls += rhs.nulls;
    return result;
  }
};

template <>
struct Reduce<Bool, Bool> {
  static Schema DoIt(const Bool& lhs, const Bool& rhs) {
    Bool result(lhs);
    result.values_false += rhs.values_false;
    result.values_true += rhs.values_true;
    result.nulls += rhs.nulls;
    return result;
  }
};

template <>
struct Reduce<ObjectOrArray, ObjectOrArray> {
  static Schema DoIt(const ObjectOrArray& lhs, const ObjectOrArray& rhs) {
    if (lhs.IsObject() != rhs.IsObject()) {
      CURRENT_THROW(InferSchemaArrayAndObjectAreIncompatible());
    }
    if (lhs.IsObject()) {
      std::vector<std::string> lhs_fields;
      std::vector<std::string> rhs_fields;
      for (const auto& cit : lhs.fields) {
        lhs_fields.push_back(cit.first);
      }
      for (const auto& cit : rhs.fields) {
        rhs_fields.push_back(cit.first);
      }
      std::vector<std::string> union_fields;
      std::set_union(lhs_fields.begin(),
                     lhs_fields.end(),
                     rhs_fields.begin(),
                     rhs_fields.end(),
                     std::back_inserter(union_fields));
      ObjectOrArray object;
      for (const auto& f : union_fields) {
        auto& intermediate = object.fields[f];
        const auto& lhs_cit = lhs.fields.find(f);
        const auto& rhs_cit = rhs.fields.find(f);
        if (lhs_cit == lhs.fields.end()) {
          CallReduce(rhs_cit->second, Null(), intermediate);
        } else if (rhs_cit == rhs.fields.end()) {
          CallReduce(lhs_cit->second, Null(), intermediate);
        } else {
          CallReduce(lhs_cit->second, rhs_cit->second, intermediate);
        }
      }
      object.instances = lhs.instances + rhs.instances;
      object.nulls = lhs.nulls + rhs.nulls;
      return object;
    } else {
      ObjectOrArray array(lhs);
      CallReduce(lhs.fields.at(""), rhs.fields.at(""), array.fields[""]);
      array.instances = lhs.instances + rhs.instances;
      array.nulls = lhs.nulls + rhs.nulls;
      return array;
    }
  }
};

inline Schema RecursivelyInferSchema(const rapidjson::Value& value) {
  // Note: Empty arrays are silently ignored. Their schema can not be inferred.
  //       If the only value for certain field is an empty array, this field will not be output.
  //       If certain field has possible values other than an empty array, they will be used.
  if (value.IsObject()) {
    ObjectOrArray object;
    for (auto cit = value.MemberBegin(); cit != value.MemberEnd(); ++cit) {
      const auto& inner = cit->value;
      if (!(inner.IsArray() && inner.Empty())) {
        const std::string key = std::string(cit->name.GetString(), cit->name.GetStringLength());
        if (!IsValidCPPIdentifier(key)) {
          CURRENT_THROW(InferSchemaInvalidCPPIdentifierException(key));
        }
        object.fields[key] = RecursivelyInferSchema(inner);
      }
    }
    return object;
  } else if (value.IsArray()) {
    if (value.Empty()) {
      CURRENT_THROW(InferSchemaTopLevelEmptyArrayIsNotAllowed());
    } else {
      ObjectOrArray object;
      bool first = true;
      for (auto cit = value.Begin(); cit != value.End(); ++cit) {
        const auto& inner = *cit;
        if (!(inner.IsArray() && inner.Empty())) {
          const auto element = RecursivelyInferSchema(inner);
          Schema& destination = object.fields[""];
          if (first) {
            first = false;
            destination = element;
          } else {
            CallReduce(destination, element, destination);
          }
        }
      }
      if (first) {
        CURRENT_THROW(InferSchemaArrayOfNullsOrEmptyArraysIsNotAllowed());
      }
      return object;
    }
  } else if (value.IsString()) {
    return String(std::string(value.GetString(), value.GetStringLength()));
  } else if (value.IsBool()) {
    Bool result;
    if (!value.IsTrue()) {
      result.values_false = 1;
    } else {
      result.values_true = 1;
    }
    return result;
  } else if (value.IsNull()) {
    return Null();
  } else {
    CURRENT_THROW(InferSchemaUnsupportedTypeException());
  }
}

inline impl::Schema SchemaFromJSON(const rapidjson::Document& document) {
  return impl::RecursivelyInferSchema(document);
}

// TODO(dkorolev): This is work in progress.
struct SchemaToCurrentStructDumper {
  const Schema& schema;
  std::ostringstream& os;
  std::string path;

  SchemaToCurrentStructDumper(const Schema& schema,
                              std::ostringstream& os,
                              const std::string& top_level_struct_name)
      : schema(schema), os(os) {
    path = top_level_struct_name;
    schema.Call(*this);
  }

  void operator()(const Null& x) { os << path << " : Null, " << x.occurrences << " occurrences." << std::endl; }

  void operator()(const String& x) {
    os << path << "\tString\t" << x.instances << '\t' << x.nulls << '\t' << x.values.size()
       << " distinct values";
    if (x.values.size() <= 8) {
      std::vector<std::pair<int, std::string>> sorted;
      for (const auto& s : x.values) {
        sorted.emplace_back(-static_cast<int>(s.second), s.first);
      }
      std::sort(sorted.rbegin(), sorted.rend());
      std::vector<std::string> sorted_as_strings;
      for (const auto& e : sorted) {
        sorted_as_strings.push_back(e.second + ':' + ToString(-e.first));
      }
      os << '\t' << current::strings::Join(sorted_as_strings, ", ");
    }
    os << std::endl;
  }

  void operator()(const Bool& x) {
    os << path << "\tBool\t" << (x.values_false + x.values_true) << '\t' << x.nulls << '\t' << x.values_false
       << " false, " << x.values_true << " true" << std::endl;
  }

  void operator()(const ObjectOrArray& x) {
    if (x.IsObject()) {
      os << path << "\tObject\t" << x.instances << '\t' << x.nulls << std::endl;
      const auto save = path;
      for (const auto& f : x.fields) {
        path = save + '.' + f.first;
        f.second.Call(*this);
      }
      path = save;
    } else {
      os << path << "\tArray\t" << x.instances << '\t' << x.nulls << std::endl;
      const auto save = path;
      path += "[]";
      x.fields.at("").Call(*this);
      path = save;
    }
  }
};

}  // namespace impl

inline impl::Schema InferRawSchemaFromJSON(const std::string& json) {
  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    CURRENT_THROW(InferSchemaParseJSONException());
  }
  return impl::SchemaFromJSON(document);
}

inline std::string InferSchemaFromJSON(const std::string& json,
                                       const std::string& top_level_struct_name = "Schema") {
  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    CURRENT_THROW(InferSchemaParseJSONException());
  }
  const impl::Schema schema = impl::SchemaFromJSON(document);
  std::ostringstream result;
  impl::SchemaToCurrentStructDumper dumper(schema, result, top_level_struct_name);
  return result.str();
}

}  // namespace utils
}  // namespace current

#endif  // CURRENT_UTILS_JSONSCHEMA_INFER_H
