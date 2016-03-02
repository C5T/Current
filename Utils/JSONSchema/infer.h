/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

// LCOV_EXCL_START
struct InferSchemaException : Exception {
  using Exception::Exception;
};

// The input string is not a valid JSON.
struct InferSchemaParseJSONException : InferSchemaException {};

// The input JSON can not be a `CURRENT_STRUCT`.
struct InferSchemaInputException : InferSchemaException {
  using InferSchemaException::InferSchemaException;
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

template <typename T_LHS, typename T_RHS>
struct InferSchemaIncompatibleTypes : InferSchemaIncompatibleTypesBase {
  InferSchemaIncompatibleTypes(const T_LHS& lhs, const T_RHS& rhs)
      : InferSchemaIncompatibleTypesBase("Incompatible types: '" + lhs.HumanReadableType() + "' and '" +
                                         rhs.HumanReadableType() + "'.") {}
};
// LCOV_EXCL_STOP

namespace impl {

inline bool IsValidCPPIdentifier(const std::string& s) {
  if (s.empty()) {
    return false;  // LCOV_EXCL_LINE
  }
  if (s == "NULL" || s == "null" || s == "true" || s == "false") {
    // TODO(dkorolev): Test for other reserved words, such as "case" or "int".
    return false;  // LCOV_EXCL_LINE
  }
  if (!(s[0] == '_' || std::isalpha(s[0]))) {
    return false;  // LCOV_EXCL_LINE
  }
  for (size_t i = 1u; i < s.length(); ++i) {
    if (!std::isalnum(s[i])) {
      return false;  // LCOV_EXCL_LINE
    }
  }
  return true;
}

// LCOV_EXCL_START
inline std::string MaybeOptionalHumanReadableType(bool has_nulls, const std::string& type) {
  return (has_nulls ? "optional " : "") + type;
}
// LCOV_EXCL_STOP

// Inferred JSON types for fields.
// Internally, the type is maintained along with the histogram of its values seen.
CURRENT_STRUCT(String) {
  CURRENT_FIELD(values, (std::map<std::string, uint32_t>));
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  CURRENT_DEFAULT_CONSTRUCTOR(String) {}
  CURRENT_CONSTRUCTOR(String)(const std::string& string) { values[string] = 1; }

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "std::string"); }
  // LCOV_EXCL_STOP
};

CURRENT_STRUCT(Bool) {
  CURRENT_FIELD(values_false, uint32_t, 0);
  CURRENT_FIELD(values_true, uint32_t, 0);
  CURRENT_FIELD(nulls, uint32_t, 0);

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "bool"); }
  // LCOV_EXCL_STOP
};

// Note: The `Null` type is largely ephemeral. Top-level "null" is still not allowed in input JSONs.
CURRENT_STRUCT(Null) { CURRENT_FIELD(occurrences, uint32_t, 1); };

CURRENT_FORWARD_DECLARE_STRUCT(Array);
CURRENT_FORWARD_DECLARE_STRUCT(Object);

using Schema = Variant<String, Bool, Null, Array, Object>;

CURRENT_STRUCT(Array) {
  CURRENT_FIELD(element, Schema);
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "array"); }
  // LCOV_EXCL_STOP
};

CURRENT_STRUCT(Object) {
  CURRENT_FIELD(fields, (std::map<std::string, Schema>));
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "object"); }
  // LCOV_EXCL_STOP
};

// `Reduce` and `CallReduce` implement the logic of building the schema for a superset of schemas.
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
struct Reduce<Object, Object> {
  static Schema DoIt(const Object& lhs, const Object& rhs) {
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
    Object object;
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
  }
};

template <>
struct Reduce<Array, Array> {
  static Schema DoIt(const Array& lhs, const Array& rhs) {
    Array array(lhs);
    CallReduce(lhs.element, rhs.element, array.element);
    array.instances = lhs.instances + rhs.instances;
    array.nulls = lhs.nulls + rhs.nulls;
    return array;
  }
};

inline Schema RecursivelyInferSchema(const rapidjson::Value& value) {
  // Note: Empty arrays are silently ignored. Their schema can not be inferred.
  //       If the only value for certain field is an empty array, this field will not be output.
  //       If certain field has possible values other than empty array, they will be used, as `Optional<>`.
  if (value.IsObject()) {
    Object object;
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
      Array array;
      bool first = true;
      for (auto cit = value.Begin(); cit != value.End(); ++cit) {
        const auto& inner = *cit;
        if (!(inner.IsArray() && inner.Empty())) {
          const auto element = RecursivelyInferSchema(inner);
          Schema& destination = array.element;
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
      return array;
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

class SchemaToTSVPrinter {
 public:
  SchemaToTSVPrinter(const Schema& schema, std::ostringstream& os, size_t number_of_example_values)
      : os_(os), path_("Schema"), number_of_example_values_(number_of_example_values) {
    os_ << "Field\tType\tSet\tUnset/Null\tValues\tDetails\n";
    schema.Call(*this);
  }

  void operator()(const Null& x) { os_ << path_ << "\tNull\t" << x.occurrences << '\n'; }

  void operator()(const String& x) {
    os_ << path_ << "\tString\t" << x.instances << '\t' << x.nulls << '\t';
    if (x.values.empty()) {
      os_ << "no values";
    } else if (x.values.size() == 1) {
      os_ << "1 distinct value";
    } else {
      os_ << x.values.size() << " distinct values";
    }
    if (x.values.size() <= number_of_example_values_) {
      // Output most common values of this [string] field, if there aren't too many of them.
      std::vector<std::pair<int, std::string>> sorted;
      for (const auto& s : x.values) {
        sorted.emplace_back(-static_cast<int>(s.second), s.first);
      }
      std::sort(sorted.begin(), sorted.end());
      std::vector<std::string> sorted_as_strings;
      for (const auto& e : sorted) {
        sorted_as_strings.push_back(e.second + " : " + current::ToString(-e.first));
      }
      os_ << '\t' << current::strings::Join(sorted_as_strings, ", ");
    }
    os_ << '\n';
  }

  void operator()(const Bool& x) {
    os_ << path_ << "\tBool\t" << (x.values_false + x.values_true) << '\t' << x.nulls << '\t' << x.values_false
        << " false, " << x.values_true << " true" << '\n';
  }

  void operator()(const Array& x) {
    const auto save_path = path_;
    os_ << path_ << "\tArray\t" << x.instances << '\t' << x.nulls << '\n';
    path_ += "[]";
    x.element.Call(*this);
    path_ = save_path;
  }

  void operator()(const Object& x) {
    const auto save_path = path_;

    std::vector<std::string> fields;
    for (const auto& f : x.fields) {
      fields.push_back(f.first);
    }

    os_ << path_ << "\tObject\t" << x.instances << '\t' << x.nulls << '\t';
    if (x.fields.empty()) {
      os_ << "empty object";
    } else if (x.fields.size() == 1) {
      os_ << "1 field";
    } else {
      os_ << x.fields.size() << " fields";
    }
    os_ << '\t' << current::strings::Join(fields, ", ") << '\n';

    for (const auto& f : x.fields) {
      path_ = save_path.empty() ? f.first : (save_path + '.' + f.first);
      f.second.Call(*this);
    }

    path_ = save_path;
  }

 private:
  std::ostringstream& os_;
  std::string path_;  // Path of the current node, changes throughout the recursive traversal.
  const size_t number_of_example_values_;
};

class SchemaToCurrentStructPrinter {
 public:
  struct Printer {
    std::ostream& os;
    const std::string prefix;
    std::string& output_type;
    std::string& output_comment;

    Printer(std::ostream& os, const std::string& prefix, std::string& output_type, std::string& output_comment)
        : os(os), prefix(prefix), output_type(output_type), output_comment(output_comment) {}

    void operator()(const Null&) {
      output_type = "";
      output_comment = "`null`-s and/or empty arrays, ignored in the schema.";
    }

    void operator()(const String& x) { output_type = x.nulls ? "Optional<std::string>" : "std::string"; }

    void operator()(const Bool& x) { output_type = x.nulls ? "Optional<bool>" : "bool"; }

    void operator()(const Array& x) {
      x.element.Call(Printer(os, prefix + "_Element", output_type, output_comment));
      output_type = "std::vector<" + output_type + ">";

      // For both arrays and objects.
      if (x.nulls) {
        output_type = "Optional<" + output_type + ">";
      }
    }

    void operator()(const Object& x) {
      output_type = prefix + "_Object";
      // [ { name, { type, comment } ].
      std::vector<std::pair<std::string, std::pair<std::string, std::string>>> output_fields;
      output_fields.reserve(x.fields.size());
      for (const auto& input_field : x.fields) {
        output_fields.resize(output_fields.size() + 1);
        auto& output_field = output_fields.back();
        std::string& field_name = output_field.first;
        std::string& type = output_field.second.first;
        std::string& comment = output_field.second.second;
        field_name = input_field.first;
        input_field.second.Call(Printer(os, prefix + '_' + field_name, type, comment));
      }
      os << '\n';
      os << "CURRENT_STRUCT(" << output_type << ") {\n";
      for (const auto& f : output_fields) {
        if (!f.second.first.empty()) {
          os << "  CURRENT_FIELD(" << f.first << ", " << f.second.first << ");";
          if (!f.second.second.empty()) {
            os << "  // " << f.second.second;
          }
          os << '\n';
        } else {
          // No type, just a comment.
          os << "  // `" << f.first << "` : " << f.second.second << '\n';
        }
      }
      os << "};\n";

      // For both arrays and objects.
      if (x.nulls) {
        output_type = "Optional<" + output_type + ">";
      }
    }
  };

  SchemaToCurrentStructPrinter(const Schema& schema,
                               std::ostringstream& os,
                               const std::string& top_level_struct_name) {
    os << "// Autogenerated schema inferred from input JSON data.\n";

    std::string type;
    std::string comment;
    schema.Call(Printer(os, top_level_struct_name, type, comment));

    os << '\n';
    if (!comment.empty()) {
      os << "// " << comment << '\n';
    }

    // This top-level `using` allows inferring schema for primitive types, such as bare strings or bare arrays.
    os << "using " << top_level_struct_name << " = " << type << ";\n";
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

inline std::string JSONSchemaAsTSV(const std::string& json, const size_t number_of_example_values = 20u) {
  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    CURRENT_THROW(InferSchemaParseJSONException());
  }
  const impl::Schema schema = impl::SchemaFromJSON(document);
  std::ostringstream result;
  impl::SchemaToTSVPrinter printer(schema, result, number_of_example_values);
  return result.str();
}

inline std::string JSONSchemaAsCurrentStructs(const std::string& json,
                                              const std::string& top_level_struct_name = "Schema") {
  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    CURRENT_THROW(InferSchemaParseJSONException());
  }
  const impl::Schema schema = impl::SchemaFromJSON(document);
  std::ostringstream result;
  impl::SchemaToCurrentStructPrinter printer(schema, result, top_level_struct_name);
  return result.str();
}

}  // namespace utils
}  // namespace current

#endif  // CURRENT_UTILS_JSONSCHEMA_INFER_H
