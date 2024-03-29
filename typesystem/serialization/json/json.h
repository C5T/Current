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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_JSON_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_JSON_H

#include "exceptions.h"
#include "rapidjson.h"

#include "../serialization.h"

#include "../../struct.h"
#include "../../optional.h"
#include "../../helpers.h"

#include "../../../bricks/strings/chunk.h"
#include "../../../bricks/template/pod.h"  // `current::copy_free`.

namespace current {
namespace serialization {
namespace json {

// SFINAE doesn't work wrt checking whether `JSON()` is possible for `T`, as the declaration is always there.
template <bool IS_CURRENT_STRUCT, bool IS_CURRENT_VARIANT, bool IS_ENUM, typename T>
struct IsJSONSerializableImpl;

template <typename T>
struct IsJSONSerializable {
  constexpr static bool value =
      IsJSONSerializableImpl<IS_CURRENT_STRUCT(T), IS_CURRENT_VARIANT(T), std::is_enum_v<T>, T>::value &&
      !std::is_same_v<T, CurrentStruct> && !std::is_same_v<T, CurrentStruct>;
};

// Helper logic for `IsJSONSerializable`.
template <typename T>
struct IsFieldJSONSerializable;

template <typename T>
struct IsFieldJSONSerializable<reflection::FieldTypeWrapper<T>> {
  constexpr static bool value = IsJSONSerializable<T>::value;
};

template <typename T, int I, int N>
struct AreCurrentStructFieldsSerializable {
  constexpr static bool value = IsFieldJSONSerializable<decltype(std::declval<T>().CURRENT_REFLECTION(
                                    reflection::Index<reflection::FieldType, I>()))>::value &&
                                AreCurrentStructFieldsSerializable<T, I + 1, N>::value;
};

template <typename T, int N>
struct AreCurrentStructFieldsSerializable<T, N, N> {
  constexpr static bool value = true;
};

template <typename T>
struct IsCurrentStructJSONSerializable {
  constexpr static bool value = IsCurrentStructJSONSerializable<reflection::SuperType<T>>::value &&
                                AreCurrentStructFieldsSerializable<T, 0, reflection::FieldCounter<T>::value>::value;
};

template <>
struct IsCurrentStructJSONSerializable<CurrentStruct> {
  constexpr static bool value = true;
};

template <typename T>
struct AreTypesInTypeListSerializable;

template <typename T, typename... TS>
struct AreTypesInTypeListSerializable<TypeListImpl<T, TS...>> {
  constexpr static bool value =
      IsJSONSerializable<T>::value && AreTypesInTypeListSerializable<TypeListImpl<TS...>>::value;
};

template <>
struct AreTypesInTypeListSerializable<TypeListImpl<>> {
  constexpr static bool value = true;
};

template <typename T>
struct IsJSONSerializableImpl<true, false, false, T> {
  constexpr static bool value = IsCurrentStructJSONSerializable<T>::value;
};

template <typename T>
struct IsJSONSerializableImpl<false, true, false, T> {
  constexpr static bool value = AreTypesInTypeListSerializable<typename T::typelist_t>::value;
};

template <typename T>
struct IsJSONSerializableImpl<false, false, true, T> {
  constexpr static bool value = IsJSONSerializable<typename std::underlying_type<T>::type>::value;
};

template <typename T>
struct IsJSONSerializableImpl<false, false, false, T> {
  constexpr static bool value = false;
};

// For RapidJSON value assignments, specifically strings (use `SetString`, not `SetValue`) and `std::chrono::*`.
template <typename T>
struct JSONValueAssignerImpl {
  static void AssignValue(rapidjson::Value& destination, current::copy_free<T> value) { destination = value; }
};

template <class JSON_FORMAT>
class JSONStringifier final {
 public:
  JSONStringifier() {
    // Can't assign in the initializer list, `current_` is the field before `document_`.
    // And I've confirmed keeping `document_` first results in performance degradation. -- D.K.
    current_ = &document_;
  }

  rapidjson::Value& Current() { return *current_; }
  rapidjson::Document::AllocatorType& Allocator() { return document_.GetAllocator(); }

  template <typename T>
  void operator=(T&& x) {
    JSONValueAssignerImpl<current::decay_t<T>>::AssignValue(*current_, std::forward<T>(x));
  }

  // Serialize another object, in an inner scope. The object is guaranteed to result in a valid value.
  template <typename T>
  void Inner(rapidjson::Value* inner_value, T&& x) {
    std::swap(current_, inner_value);
    Serialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
  }

  // Serialize another object, in an inner scope. The object may end up a no-op, which should be ignored.
  // Example: A `Variant` or `Optional` in the `Minimalistic` format.
  void MarkAsAbsentValue() { current_ = nullptr; }
  template <typename T>
  bool MaybeInner(rapidjson::Value* inner_value, T&& x) {
    rapidjson::Value* previous = current_;
    current_ = inner_value;
    Serialize(*this, std::forward<T>(x));
    if (current_) {
      current_ = previous;
      return true;
    } else {
      current_ = previous;
      return false;
    }
  }

  std::string ResultingJSON() const {
    rapidjson::StringBuffer string_buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
    document_.Accept(writer);
    return string_buffer.GetString();
  }

 private:
  rapidjson::Value* current_;
  rapidjson::Document document_;
};

enum class JSONVariantStyle : int { Current, Simple, NewtonsoftFSharp };

template <JSONVariantStyle>
struct JSONVariantStyleUseNulls {
  constexpr static bool value = true;
};
template <>
struct JSONVariantStyleUseNulls<JSONVariantStyle::Simple> {
  constexpr static bool value = false;
};

struct JSONFormat {
  struct Current {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Current;
  };
  struct Minimalistic {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Simple;
  };
  struct JavaScript {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::Simple;
  };
  struct NewtonsoftFSharp {
    constexpr static JSONVariantStyle variant_style = JSONVariantStyle::NewtonsoftFSharp;
  };
};

template <class>
struct JSONVariantTypeIDInEmptyKey {
  constexpr static bool value = false;
};
template <>
struct JSONVariantTypeIDInEmptyKey<JSONFormat::Current> {
  constexpr static bool value = true;
};

template <class>
struct JSONVariantTypeNameInDollarKey {
  constexpr static bool value = false;
};
template <>
struct JSONVariantTypeNameInDollarKey<JSONFormat::JavaScript> {
  constexpr static bool value = true;
};

template <class J>
struct JSONPatcher {
  constexpr static JSONVariantStyle variant_style = J::variant_style;
};

template <class J>
struct JSONPatchMode {
  constexpr static bool value = false;
};

template <class J>
struct JSONPatchMode<JSONPatcher<J>> {
  constexpr static bool value = true;
};

template <class JSON_FORMAT>
class JSONParser final {
 public:
  explicit JSONParser(const char* json) {
    if (document_.Parse(json).HasParseError()) {
      CURRENT_THROW(InvalidJSONException(json));
    }
    current_ = &document_;
  }

  operator bool() const { return current_ != nullptr; }
  rapidjson::Value& Current() { return *current_; }
  rapidjson::Value* CurrentAsPtr() { return current_; }

  template <typename T>
  void Inner(rapidjson::Value* inner_value, T&& x) {
    std::swap(current_, inner_value);
    Deserialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
  }

  template <typename T, typename P1>
  void Inner(rapidjson::Value* inner_value, T&& x, P1 p1) {
    path_.emplace_back(p1);
    std::swap(current_, inner_value);
    Deserialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
    path_.pop_back();
  }

  // The `P1, P2, P3` and `CharPtrOrInt` magic are optimizations for fast JSON path construction. -- D.K.
  template <typename T, typename P1, typename P2>
  void Inner(rapidjson::Value* inner_value, T&& x, P1 p1, P2 p2) {
    path_.emplace_back(p1);
    path_.emplace_back(p2);
    std::swap(current_, inner_value);
    Deserialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
    path_.pop_back();
    path_.pop_back();
  }

  template <typename T, typename P1, typename P2, typename P3>
  void Inner(rapidjson::Value* inner_value, T&& x, P1 p1, P2 p2, P3 p3) {
    path_.emplace_back(p1);
    path_.emplace_back(p2);
    path_.emplace_back(p3);
    std::swap(current_, inner_value);
    Deserialize(*this, std::forward<T>(x));
    std::swap(current_, inner_value);
    path_.pop_back();
    path_.pop_back();
    path_.pop_back();
  }

  struct CharPtrOrInt {
    const char* p;
    int i;
    CharPtrOrInt(const char* p) : p(p) {}
    CharPtrOrInt(int i) : p(nullptr), i(i) {}
    void AppendToString(std::string& s) const {
      if (p) {
        s.append(p);
      } else {
        s.append(current::ToString(i));
      }
    }
  };

  bool PathIsEmpty() const { return path_.empty(); }

  std::string Path() const {
    std::string path;
    for (const CharPtrOrInt& p : path_) {
      p.AppendToString(path);
    }
    return path[0] != '.' ? path : path.substr(1u);
  }

 private:
  rapidjson::Value* current_;
  std::vector<CharPtrOrInt> path_;
  rapidjson::Document document_;
};

template <class J, typename T>
void ParseJSONViaRapidJSON(const char* json, T& destination) {
  JSONParser<J> json_parser(json);
  Deserialize(json_parser, destination);
}

template <class J = JSONFormat::Current, typename T>
inline std::string JSON(const T& source) {
  JSONStringifier<J> json_stringifier;
  Serialize(json_stringifier, source);
  return json_stringifier.ResultingJSON();
}

template <class J = JSONFormat::Current>
inline std::string JSON(const char* special_case_bare_c_string) {
  return JSON<J>(std::string(special_case_bare_c_string));
}

template <typename T, class J = JSONFormat::Current>
inline void ParseJSON(const char* source, T& destination) {
  try {
    ParseJSONViaRapidJSON<J>(source, destination);
    CheckIntegrity(destination);
  } catch (UninitializedVariant) {
    CURRENT_THROW(JSONUninitializedVariantObjectException());
  }
}

template <typename T, class J = JSONFormat::Current>
inline void ParseJSON(const std::string& source, T& destination) {
  ParseJSON(source.c_str(), destination);
}

template <typename T, class J = JSONFormat::Current>
inline void ParseJSON(const strings::Chunk& source, T& destination) {
  ParseJSON(source.c_str(), destination);
}

template <typename T, class J = JSONFormat::Current>
inline void PatchObjectWithJSON(T& object, const char* json) {
  try {
    CheckIntegrity(object);  // TODO(dkorolev): Different exception for "was uninitialized before"?
    ParseJSONViaRapidJSON<JSONPatcher<J>>(json, object);
    CheckIntegrity(object);
  } catch (UninitializedVariant) {
    CURRENT_THROW(JSONUninitializedVariantObjectException());
  }
}

template <typename T, class J = JSONFormat::Current>
inline void PatchObjectWithJSON(T& object, const std::string& json) {
  PatchObjectWithJSON<T, J>(object, json.c_str());
}

template <typename T, class J = JSONFormat::Current>
inline void PatchObjectWithJSON(T& object, const strings::Chunk& json) {
  PatchObjectWithJSON<T, J>(object, json.c_str());
}

template <typename T, class J = JSONFormat::Current>
inline T ParseJSON(const char* source) {
  T result;
  ParseJSON<T, J>(source, result);
  return result;
}

template <typename T, class J = JSONFormat::Current>
inline T ParseJSON(const std::string& source) {
  return ParseJSON<T, J>(source.c_str());
}

template <typename T, class J = JSONFormat::Current>
inline T ParseJSON(const strings::Chunk& source) {
  return ParseJSON<T, J>(source.c_str());
}

template <typename T, class J = JSONFormat::Current>
inline Optional<T> TryParseJSON(const char* source) {
  try {
    T result;
    ParseJSON<T, J>(source, result);
    return result;
  } catch (const TypeSystemParseJSONException&) {
    return nullptr;
  }
}

template <typename T, class J = JSONFormat::Current>
inline Optional<T> TryParseJSON(const std::string& source) {
  return TryParseJSON<T, J>(source.c_str());
}

template <typename T, class J = JSONFormat::Current>
inline Optional<T> TryParseJSON(const strings::Chunk& source) {
  return TryParseJSON<T, J>(source.c_str());
}

}  // namespace json
}  // namespace serialization

// Keep top-level symbols both in `current::` and in global namespace.
using serialization::json::InvalidJSONException;
using serialization::json::JSON;
using serialization::json::JSONFormat;
using serialization::json::JSONSchemaException;
using serialization::json::JSONUninitializedVariantObjectException;
using serialization::json::ParseJSON;
using serialization::json::PatchObjectWithJSON;
using serialization::json::TryParseJSON;
using serialization::json::TypeSystemParseJSONException;
}  // namespace current

using current::InvalidJSONException;
using current::JSON;
using current::JSONFormat;
using current::JSONSchemaException;
using current::JSONUninitializedVariantObjectException;
using current::ParseJSON;
using current::PatchObjectWithJSON;
using current::TryParseJSON;
using current::TypeSystemParseJSONException;

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_JSON_H
