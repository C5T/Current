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

#include "../../typesystem/struct.h"
#include "../../typesystem/schema/schema.h"
#include "../../typesystem/serialization/json.h"

#include "../../bricks/file/file.h"

namespace current {
namespace utils {

struct DoNotTrackPath {
  DoNotTrackPath() = default;
  DoNotTrackPath Member(const std::string& unused_name) const {
    static_cast<void>(unused_name);
    return DoNotTrackPath();
  }
  DoNotTrackPath ArrayElement(size_t unused_index) const {
    static_cast<void>(unused_index);
    return DoNotTrackPath();
  }
  operator bool() const { return true; }
};

struct TrackPathIgnoreList {
  std::unordered_set<std::string> ignore_list;
  TrackPathIgnoreList(const std::string& value) {
    for (auto&& path : strings::Split(value, ":;, \t\n")) {
      ignore_list.emplace(std::move(path));
    }
  }
  bool IsIgnored(const std::string& path) const { return ignore_list.count(path) ? true : false; }
};

struct TrackPath {
  const std::string path;
  const TrackPathIgnoreList* ignore = nullptr;
  TrackPath() = default;
  TrackPath(const TrackPathIgnoreList& ignore) : ignore(&ignore) {}
  TrackPath(const std::string& path, const TrackPathIgnoreList* ignore) : path(path), ignore(ignore) {}
  TrackPath Member(const std::string& name) const { return TrackPath(path + '.' + name, ignore); }
  TrackPath ArrayElement(size_t index) const { return TrackPath(path + '[' + ToString(index) + ']', ignore); }
  operator bool() const { return ignore ? !ignore->IsIgnored(path) : true; }
};

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
  template <typename PATH>
  explicit InferSchemaInvalidCPPIdentifierException(const std::string& id, const PATH&)
      : InferSchemaInputException("Invalid C++ identifier used as the key in JSON: '" + id + "'.") {}
  explicit InferSchemaInvalidCPPIdentifierException(const std::string& id, const TrackPath& path)
      : InferSchemaInputException("Invalid C++ identifier used as the key in JSON: '" + id + "', at '" + path.path +
                                  "'.") {}
};

struct InferSchemaIncompatibleTypesBase : InferSchemaInputException {
  InferSchemaIncompatibleTypesBase() : InferSchemaInputException("Incompatible types.") {}
  explicit InferSchemaIncompatibleTypesBase(const std::string& message) : InferSchemaInputException(message) {}
};

template <typename LHS, typename RHS>
struct InferSchemaIncompatibleTypes : InferSchemaIncompatibleTypesBase {
  InferSchemaIncompatibleTypes(const LHS& lhs, const RHS& rhs)
      : InferSchemaIncompatibleTypesBase("Incompatible types: '" + lhs.HumanReadableType() + "' and '" +
                                         rhs.HumanReadableType() + "'.") {}
};
// LCOV_EXCL_STOP

namespace impl {

constexpr static size_t kNumberOfUniqueValuesToTrack = 100u;

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
    if (!(s[i] == '_' || std::isalnum(s[i]))) {
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
  CURRENT_FIELD(values, (std::unordered_map<std::string, uint32_t>));
  CURRENT_FIELD(counters, (std::set<std::pair<uint32_t, std::string>>));
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  CURRENT_DEFAULT_CONSTRUCTOR(String) {}
  CURRENT_CONSTRUCTOR(String)(const std::string& string) {
    values[string] = 1;
    counters.emplace(1, string);
  }

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "std::string"); }
  // LCOV_EXCL_STOP
};

CURRENT_STRUCT(Integer) {
  CURRENT_FIELD(sum, int64_t, 0);           // Assume the sum would fit an int64. Because why not? -- D.K.
  CURRENT_FIELD(sum_squares, double, 0.0);  // Need double as squares of large numbers won't fit.
  CURRENT_FIELD(can_be_unsigned, bool, true);
  CURRENT_FIELD(can_be_microseconds, bool, false);
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  CURRENT_DEFAULT_CONSTRUCTOR(Integer) {}
  CURRENT_CONSTRUCTOR(Integer)(int64_t value) {
    sum = value;
    sum_squares = 1.0 * value * value;
    can_be_unsigned = (value >= 0);
    // clang-format off
    const int64_t year_1980 =  315561600ll * 1000000ll;  // $(date -d "Jan 1 1980" +%s)
    const int64_t year_2250 = 8835984000ll * 1000000ll;  // $(date -d "Jan 1 2250" +%s)
    // clang-format on
    can_be_microseconds = (value >= year_1980 && value < year_2250);
  }

  std::string CPPType() const {
    // TODO(dkorolev): Integers of less than 64 bits?
    return can_be_microseconds ? "std::chrono::microseconds" : can_be_unsigned ? "uint64_t" : "int64_t";
  }

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, CPPType()); }
  // LCOV_EXCL_STOP
};

CURRENT_STRUCT(Double) {
  CURRENT_FIELD(sum, double, 0);
  CURRENT_FIELD(sum_squares, double, 0.0);
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  CURRENT_DEFAULT_CONSTRUCTOR(Double) {}
  CURRENT_CONSTRUCTOR(Double)(double value) {
    sum = value;
    sum_squares = value * value;
  }
  CURRENT_CONSTRUCTOR(Double)(const Integer& integer) {
    sum = static_cast<double>(integer.sum);
    sum_squares = integer.sum_squares;
    instances = integer.instances;
    nulls = integer.nulls;
  }

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "double"); }
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

// Note: `Uninitialized` is a special type. It's introduced to make folds functionally uniform,
// eliminating the need for an extra `bool first` variable.
// clang-format off
CURRENT_STRUCT(Uninitialized) {
  std::string HumanReadableType() const {
    return "Uninitialized";
  }
};
// clang-format on

// Note: The `Null` type is largely ephemeral. Top-level "null" is still not allowed in input JSONs.
CURRENT_STRUCT(Null) { CURRENT_FIELD(occurrences, uint32_t, 1); };

CURRENT_FORWARD_DECLARE_STRUCT(Array);
CURRENT_FORWARD_DECLARE_STRUCT(Object);

using Schema = Variant<Uninitialized, String, Integer, Double, Bool, Null, Array, Object>;

CURRENT_STRUCT(Array) {
  CURRENT_FIELD(element, Schema, Uninitialized());
  CURRENT_FIELD(instances, uint32_t, 1);
  CURRENT_FIELD(nulls, uint32_t, 0);

  // LCOV_EXCL_START
  std::string HumanReadableType() const { return MaybeOptionalHumanReadableType(nulls, "array"); }
  // LCOV_EXCL_STOP
};

CURRENT_STRUCT(Object) {
  CURRENT_FIELD(field_schema, (std::vector<std::pair<std::string, Schema>>));
  CURRENT_FIELD(field_index, (std::unordered_map<std::string, uint32_t>));
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

template <typename LHS, typename RHS>
struct Reduce {
  static Schema DoIt(const LHS& lhs, const RHS& rhs) {
    using InferSchemaIncompatibleTypesException = InferSchemaIncompatibleTypes<LHS, RHS>;
    CURRENT_THROW(InferSchemaIncompatibleTypesException(lhs, rhs));
  }
};

template <typename LHS>
struct RHSExpander {
  const LHS& lhs;
  Schema& result;
  RHSExpander(const LHS& lhs, Schema& result) : lhs(lhs), result(result) {}

  template <typename RHS>
  void operator()(const RHS& rhs) {
    result = Reduce<LHS, RHS>::DoIt(lhs, rhs);
  }

  void operator()(const Uninitialized&) { result = lhs; }
};

template <>
struct RHSExpander<Uninitialized> {
  Schema& result;
  RHSExpander(const Uninitialized&, Schema& result) : result(result) {}

  template <typename RHS>
  void operator()(const RHS& rhs) {
    result = rhs;
  }
};

struct LHSExpander {
  const Schema& rhs;
  Schema& result;
  LHSExpander(const Schema& rhs, Schema& result) : rhs(rhs), result(result) {}

  template <typename LHS>
  void operator()(const LHS& lhs) const {
    rhs.Call(RHSExpander<LHS>(lhs, result));
  }
};

inline Schema CallReduce(const Schema& lhs, const Schema& rhs) {
  Schema result;
  lhs.Call(LHSExpander(rhs, result));
  return result;
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
    result.counters.clear();
    for (const auto& counter : result.values) {
      result.counters.emplace(counter.second, counter.first);
    }
    // Keep the "distinct values" map of of manageable size.
    while (result.counters.size() > kNumberOfUniqueValuesToTrack) {
      auto iterator = result.counters.begin();
      result.values.erase(iterator->second);
      result.counters.erase(iterator);
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
struct Reduce<Integer, Integer> {
  static Schema DoIt(const Integer& lhs, const Integer& rhs) {
    Integer result(lhs);
    result.sum += rhs.sum;
    result.sum_squares += rhs.sum_squares;
    result.can_be_unsigned &= rhs.can_be_unsigned;
    result.can_be_microseconds &= rhs.can_be_microseconds;
    result.instances += rhs.instances;
    result.nulls += rhs.nulls;
    return result;
  }
};

template <>
struct Reduce<Double, Double> {
  static Schema DoIt(const Double& lhs, const Double& rhs) {
    Double result(lhs);
    result.sum += rhs.sum;
    result.sum_squares += rhs.sum_squares;
    result.instances += rhs.instances;
    result.nulls += rhs.nulls;
    return result;
  }
};

template <>
struct Reduce<Integer, Double> {
  static Schema DoIt(const Integer& lhs, const Double& rhs) { return Reduce<Double, Double>::DoIt(lhs, rhs); }
};

template <>
struct Reduce<Double, Integer> {
  static Schema DoIt(const Double& lhs, const Integer& rhs) { return Reduce<Double, Double>::DoIt(lhs, rhs); }
};

template <>
struct Reduce<Object, Object> {
  static Schema DoIt(const Object& lhs, const Object& rhs) {
    std::vector<std::string> union_fields;
    for (const auto& lhs_field : lhs.field_schema) {
      const auto& name = lhs_field.first;
      union_fields.push_back(name);
    }
    for (const auto& rhs_field : rhs.field_schema) {
      const auto& name = rhs_field.first;
      if (!lhs.field_index.count(name)) {
        union_fields.push_back(name);
      }
    }
    const size_t n = union_fields.size();
    Object object;
    for (size_t i = 0; i < n; ++i) {
      object.field_index[union_fields[i]] = static_cast<uint32_t>(i);
    }
    object.field_schema.resize(n);
    for (size_t i = 0; i < n; ++i) {
      const auto& f = union_fields[i];
      object.field_schema[i].first = f;
      auto& intermediate = object.field_schema[i].second;
      const auto& lhs_cit = lhs.field_index.find(f);
      const auto& rhs_cit = rhs.field_index.find(f);
      if (lhs_cit == lhs.field_index.end()) {
        intermediate = CallReduce(Null(), rhs.field_schema[rhs_cit->second].second);
      } else if (rhs_cit == rhs.field_index.end()) {
        intermediate = CallReduce(lhs.field_schema[lhs_cit->second].second, Null());
      } else {
        intermediate = CallReduce(lhs.field_schema[lhs_cit->second].second, rhs.field_schema[rhs_cit->second].second);
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
    Array array;
    array.element = CallReduce(lhs.element, rhs.element);
    array.instances = lhs.instances + rhs.instances;
    array.nulls = lhs.nulls + rhs.nulls;
    return array;
  }
};

template <typename PATH>
inline Schema RecursivelyInferSchema(const rapidjson::Value& value, const PATH& path) {
  // Note: Empty arrays are silently ignored. Their schema can not be inferred.
  //       If the only value for certain field is an empty array, this field will not be output.
  //       If certain field has possible values other than empty array, they will be used, as `Optional<>`.
  if (value.IsObject()) {
    Object object;
    for (auto cit = value.MemberBegin(); cit != value.MemberEnd(); ++cit) {
      const auto& inner = cit->value;
      if (!(inner.IsArray() && inner.Empty())) {
        const std::string key = std::string(cit->name.GetString(), cit->name.GetStringLength());
        const PATH next_path = path.Member(key);
        if (next_path) {
          if (!IsValidCPPIdentifier(key)) {
            CURRENT_THROW(InferSchemaInvalidCPPIdentifierException(key, path));
          }
          object.field_index[key] = static_cast<uint32_t>(object.field_schema.size());
          object.field_schema.emplace_back(key, std::move(RecursivelyInferSchema(inner, next_path)));
        }
      }
    }
    return object;
  } else if (value.IsArray()) {
    if (value.Empty()) {
      CURRENT_THROW(InferSchemaTopLevelEmptyArrayIsNotAllowed());
    } else {
      Array array;
      size_t array_index = 0u;
      for (auto cit = value.Begin(); cit != value.End(); ++cit, ++array_index) {
        const auto& inner = *cit;
        if (!(inner.IsArray() && inner.Empty())) {
          const auto element = RecursivelyInferSchema(inner, path.ArrayElement(array_index));
          array.element = CallReduce(array.element, element);
        }
      }
      if (Exists<Uninitialized>(array.element)) {
        CURRENT_THROW(InferSchemaArrayOfNullsOrEmptyArraysIsNotAllowed());
      }
      return array;
    }
  } else if (value.IsString()) {
    return String(std::string(value.GetString(), value.GetStringLength()));
  } else if (value.IsNumber()) {
    if (value.IsInt64()) {
      return Integer(value.GetInt64());
    } else {
      return Double(value.GetDouble());
    }
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

class HumanReadableSchemaExporter {
 public:
  HumanReadableSchemaExporter(const Schema& schema, std::ostringstream& os, size_t number_of_example_values)
      : os_(os), path_("Schema"), number_of_example_values_(number_of_example_values) {
    os_ << "Field\tType\tSet\tUnset/Null\tValues\tDetails\n";
    schema.Call(*this);
  }

  void operator()(const Uninitialized&) { os_ << path_ << "\tUninitialized\tN/A\n"; }

  void operator()(const Null& x) { os_ << path_ << "\tNull\t" << x.occurrences << '\n'; }

  void operator()(const String& x) {
    os_ << path_ << "\tString\t" << x.instances << '\t' << x.nulls << '\t';
    if (x.values.empty()) {
      os_ << "no values";
    } else if (x.values.size() == 1) {
      os_ << "1 distinct value";
    } else if (x.values.size() < kNumberOfUniqueValuesToTrack) {
      os_ << x.values.size() << " distinct values";
    } else {
      os_ << x.values.size() << "++ distinct values";
    }
    // Dump the most common values for this [string] field, only the top ones if there aren't too many of them.
    size_t total = 0u;
    for (auto rit = x.counters.rbegin(); rit != x.counters.rend(); ++rit) {
      os_ << ", " << JSON(rit->second) << ':' << rit->first;  // `JSON()` to quote the string and escape '\n' and '\t'.
      ++total;
      if (total >= number_of_example_values_) {
        break;
      }
    }
    os_ << '\n';
  }

  template <typename T>
  void DumpMeanAndStddev(const T& x) {
    const uint64_t n = x.instances;
    const double mean = 1.0 * x.sum / n;
    const double stddev = std::sqrt((1.0 * x.sum_squares / n) - (mean * mean));
    os_ << "\tMean " << mean << ", StdDev " << stddev << '\n';
  }

  void operator()(const Integer& x) {
    os_ << path_ << "\tInteger\t" << x.instances << '\t' << x.nulls;
    DumpMeanAndStddev(x);
  }

  void operator()(const Double& x) {
    os_ << path_ << "\tDouble\t" << x.instances << '\t' << x.nulls;
    DumpMeanAndStddev(x);
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

    os_ << path_ << "\tObject\t" << x.instances << '\t' << x.nulls << '\t';
    if (x.field_schema.empty()) {
      os_ << "empty object";
    } else if (x.field_schema.size() == 1) {
      os_ << "1 field";
    } else {
      os_ << x.field_schema.size() << " fields";
    }

    std::vector<std::string> field_names;
    field_names.reserve(x.field_schema.size());
    for (const auto& f : x.field_schema) {
      field_names.emplace_back(f.first);
    }
    os_ << '\t' << strings::Join(field_names, ", ") << '\n';

    for (const auto& field : field_names) {
      path_ = save_path.empty() ? field : (save_path + '.' + field);
      x.field_schema[x.field_index.at(field)].second.Call(*this);
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

    void operator()(const Uninitialized&) {
      output_type = "";
      output_comment = "uninitialized node detected, ignored in the schema.";
    }

    void operator()(const Null&) {
      output_type = "";
      output_comment = "`null`-s and/or empty arrays, ignored in the schema.";
    }

    void operator()(const String& x) { output_type = x.nulls ? "Optional<std::string>" : "std::string"; }

    void operator()(const Integer& x) {
      output_type = x.CPPType();
      if (x.nulls) {
        output_type = "Optional<" + output_type + '>';
      }
    }

    void operator()(const Double& x) { output_type = x.nulls ? "Optional<double>" : "double"; }

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
      output_fields.reserve(x.field_schema.size());
      for (const auto& input_field : x.field_schema) {
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

  void Print(const Schema& schema, std::ostringstream& os, const std::string& top_level_struct_name) {
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

template <typename PATH = DoNotTrackPath>
inline Schema SchemaFromOneJSON(const std::string& json, const PATH& path = PATH()) {
  rapidjson::Document document;
  if (document.Parse<0>(json.c_str()).HasParseError()) {
    CURRENT_THROW(InferSchemaParseJSONException());
  }
  return impl::RecursivelyInferSchema(document, path);
}

template <typename PATH = DoNotTrackPath>
inline Schema SchemaFromOneJSONPerLineFile(const std::string& file_name, const PATH& path = PATH()) {
  bool first = true;
  impl::Schema schema;
  FileSystem::ReadFileByLines(file_name,
                              [&path, &first, &schema](std::string&& json) {
                                rapidjson::Document document;
                                // `&json[0]` to pass a mutable string.
                                if (document.Parse<0>(&json[0]).HasParseError()) {
                                  CURRENT_THROW(InferSchemaParseJSONException());
                                }
                                if (first) {
                                  schema = impl::RecursivelyInferSchema(document, path);
                                  first = false;
                                } else {
                                  schema = CallReduce(schema, impl::RecursivelyInferSchema(document, path));
                                }
                              });
  return schema;
}

}  // namespace impl

template <typename PATH = DoNotTrackPath>
inline std::string DescribeSchema(const std::string& file_name,
                                  const PATH& path = PATH(),
                                  const size_t number_of_example_values = 20u) {
  std::ostringstream result;
  impl::HumanReadableSchemaExporter exporter(
      impl::SchemaFromOneJSONPerLineFile(file_name, path), result, number_of_example_values);
  return result.str();
}

template <typename PATH = DoNotTrackPath>
inline std::string JSONSchemaAsCurrentStructs(const std::string& file_name,
                                              const PATH& path = PATH(),
                                              const std::string& top_level_struct_name = "Schema") {
  std::ostringstream result;
  impl::SchemaToCurrentStructPrinter().Print(
      impl::SchemaFromOneJSONPerLineFile(file_name, path), result, top_level_struct_name);
  return result.str();
}

}  // namespace utils
}  // namespace current

#endif  // CURRENT_UTILS_JSONSCHEMA_INFER_H
