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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PRIMITIVES_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PRIMITIVES_H

#include <string>
#include <chrono>
#include <type_traits>

#include "json.h"

namespace current {
namespace serialization {

namespace json {
template <>
struct JSONValueAssignerImpl<std::string> {
  static void AssignValue(rapidjson::Value& destination, const std::string& value) {
    destination = rapidjson::StringRef(value);
  }
};

template <>
struct JSONValueAssignerImpl<std::chrono::microseconds> {
  static void AssignValue(rapidjson::Value& destination, std::chrono::microseconds value) {
    destination.SetInt64(value.count());
  }
};

template <>
struct JSONValueAssignerImpl<std::chrono::milliseconds> {
  static void AssignValue(rapidjson::Value& destination, std::chrono::milliseconds value) {
    destination.SetInt64(value.count());
  }
};
}  // namespace json

#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  template <class JSON_FORMAT>                                                                                  \
  struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, cpp_type> {                                          \
    static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, copy_free<cpp_type> value) {  \
      json_stringifier = value;                                                                                 \
    }                                                                                                           \
  };                                                                                                            \
  namespace json {                                                                                              \
  template <>                                                                                                   \
  struct IsJSONSerializable<cpp_type> {                                                                         \
    constexpr static bool value = true;                                                                         \
  };                                                                                                            \
  }
#include "../../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

// Parse `uint*_t`.
template <class JSON_FORMAT, typename T>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>,
                       T,
                       std::enable_if_t<std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed &&
                                        !std::is_same_v<T, bool>>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, T& destination) {
    if (json_parser && json_parser.Current().IsUint64()) {
      destination = static_cast<T>(json_parser.Current().GetUint64());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsUint64())) {
      CURRENT_THROW(JSONSchemaException("unsigned integer", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `int*_t`
template <class JSON_FORMAT, typename T>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>,
                       T,
                       std::enable_if_t<std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, T& destination) {
    if (json_parser && json_parser.Current().IsInt64()) {
      destination = static_cast<T>(json_parser.Current().GetInt64());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsInt64())) {
      CURRENT_THROW(JSONSchemaException("integer", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `float`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, float> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, float& destination) {
    if (json_parser && json_parser.Current().IsNumber()) {
      destination = static_cast<float>(json_parser.Current().GetDouble());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !(json_parser.Current().IsNumber()))) {
      CURRENT_THROW(JSONSchemaException("float", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `double`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, double> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, double& destination) {
    if (json_parser && json_parser.Current().IsNumber()) {
      destination = json_parser.Current().GetDouble();
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsNumber())) {
      CURRENT_THROW(JSONSchemaException("double", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `std::string`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::string> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::string& destination) {
    if (json_parser && json_parser.Current().IsString()) {
      destination.assign(json_parser.Current().GetString(), json_parser.Current().GetStringLength());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsString())) {
      CURRENT_THROW(JSONSchemaException("string", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `bool`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, bool> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, bool& destination) {
    if (json_parser && (json_parser.Current().IsTrue() || json_parser.Current().IsFalse())) {
      destination = json_parser.Current().IsTrue();
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value ||
               (json_parser && !(json_parser.Current().IsTrue() || json_parser.Current().IsFalse()))) {
      CURRENT_THROW(JSONSchemaException("bool", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `std::chrono::milliseconds`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::chrono::milliseconds> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::chrono::milliseconds& destination) {
    if (json_parser && json_parser.Current().IsInt64()) {
      destination = std::chrono::milliseconds(json_parser.Current().GetInt64());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsInt64())) {
      CURRENT_THROW(JSONSchemaException("milliseconds as integer", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

// `std::chrono::microseconds`.
template <class JSON_FORMAT>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::chrono::microseconds> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::chrono::microseconds& destination) {
    if (json_parser && json_parser.Current().IsInt64()) {
      destination = std::chrono::microseconds(json_parser.Current().GetInt64());
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsInt64())) {
      CURRENT_THROW(JSONSchemaException("microseconds as integer", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PRIMITIVES_H
