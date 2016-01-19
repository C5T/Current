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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PRIMITIVE_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PRIMITIVE_H

#include "../json.h"

namespace current {
namespace serialization {
namespace json {

template <>
struct AssignToRapidJSONValueImpl<std::string> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::string& value) {
    destination = rapidjson::StringRef(value);
  }
};

namespace save {

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, unused_current_type, unused_fsharp_type) \
  template <JSONFormat J>                                                                                      \
  struct SaveIntoJSONImpl<cpp_type, J> {                                                                       \
    static bool Save(rapidjson::Value& destination,                                                            \
                     rapidjson::Document::AllocatorType&,                                                      \
                     const cpp_type& value) {                                                                  \
      AssignToRapidJSONValue(destination, value);                                                              \
      return true;                                                                                             \
    }                                                                                                          \
  };
#include "../../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

}  // namespace save

namespace load {

// `uint*_t`.
template <typename T, JSONFormat J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed &&
                   !std::is_same<T, bool>::value>> {
  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<T>(source->GetUint64());
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `int*_t`
template <typename T, JSONFormat J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed>> {
  static void Load(
      rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<T>(source->GetInt64());
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `float`.
template <JSONFormat J>
struct LoadFromJSONImpl<float, J> {
  static void Load(rapidjson::Value* source, float& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<float>(source->GetDouble());
    } else {
      throw JSONSchemaException("float", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `double`.
template <JSONFormat J>
struct LoadFromJSONImpl<double, J> {
  static void Load(rapidjson::Value* source, double& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = source->GetDouble();
    } else {
      throw JSONSchemaException("double", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `std::string`.
template <JSONFormat J>
struct LoadFromJSONImpl<std::string, J> {
  static void Load(rapidjson::Value* source, std::string& destination, const std::string& path) {
    if (source && source->IsString()) {
      destination.assign(source->GetString(), source->GetStringLength());
    } else {
      throw JSONSchemaException("string", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<bool, J> {
  static void Load(rapidjson::Value* source, bool& destination, const std::string& path) {
    if (source && (source->IsTrue() || source->IsFalse())) {
      destination = source->IsTrue();
    } else {
      throw JSONSchemaException("bool", source, path);  // LCOV_EXCL_LINE
    }
  }
};
}  // namespace load

}  // namespace json
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PRIMITIVE_H


