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

#include <string>
#include <chrono>

#include "../base.h"

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

}  // namespace current::serialization::json

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, ignore_h, ignore_fs, ignore_md)          \
  template <class JSON_FORMAT>                                                                                 \
  struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, cpp_type> {                                         \
    static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, copy_free<cpp_type> value) { \
      json_stringifier = value;                                                                                \
    }                                                                                                          \
  };
#include "../../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

namespace json {
namespace load {

// `uint*_t`.
template <typename T, class J>
struct LoadFromJSONImpl<T,
                        J,
                        ENABLE_IF<std::numeric_limits<T>::is_integer && !std::numeric_limits<T>::is_signed &&
                                  !std::is_same<T, bool>::value>> {
  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsUint64()) {
      destination = static_cast<T>(source->GetUint64());
    } else if (!JSONPatchMode<J>::value || (source && !source->IsUint64())) {
      throw JSONSchemaException("unsigned integer", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `int*_t`
template <typename T, class J>
struct LoadFromJSONImpl<T,
                        J,
                        ENABLE_IF<std::numeric_limits<T>::is_integer && std::numeric_limits<T>::is_signed>> {
  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsInt64()) {
      destination = static_cast<T>(source->GetInt64());
    } else if (!JSONPatchMode<J>::value || (source && !source->IsInt64())) {
      throw JSONSchemaException("integer", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `float`.
template <class J>
struct LoadFromJSONImpl<float, J> {
  static void Load(rapidjson::Value* source, float& destination, const std::string& path) {
    if (source && source->IsDouble()) {
      destination = static_cast<float>(source->GetDouble());
    } else if (!JSONPatchMode<J>::value || (source && !(source->IsDouble()))) {
      throw JSONSchemaException("float", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `double`.
template <class J>
struct LoadFromJSONImpl<double, J> {
  static void Load(rapidjson::Value* source, double& destination, const std::string& path) {
    if (source && source->IsDouble()) {
      destination = source->GetDouble();
    } else if (!JSONPatchMode<J>::value || (source && !source->IsDouble())) {
      throw JSONSchemaException("double", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `std::string`.
template <class J>
struct LoadFromJSONImpl<std::string, J> {
  static void Load(rapidjson::Value* source, std::string& destination, const std::string& path) {
    if (source && source->IsString()) {
      destination.assign(source->GetString(), source->GetStringLength());
    } else if (!JSONPatchMode<J>::value || (source && !source->IsString())) {
      throw JSONSchemaException("string", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `bool`.
template <class J>
struct LoadFromJSONImpl<bool, J> {
  static void Load(rapidjson::Value* source, bool& destination, const std::string& path) {
    if (source && (source->IsTrue() || source->IsFalse())) {
      destination = source->IsTrue();
    } else if (!JSONPatchMode<J>::value || (source && !(source->IsTrue() || source->IsFalse()))) {
      throw JSONSchemaException("bool", source, path);  // LCOV_EXCL_LINE
    }
  }
};

// `std::chrono::milliseconds`.
template <class J>
struct LoadFromJSONImpl<std::chrono::milliseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::milliseconds& destination, const std::string& path) {
    int64_t value_as_int64;
    LoadFromJSONImpl<int64_t, J>::Load(source, value_as_int64, path);
    destination = std::chrono::milliseconds(value_as_int64);
  }
};

// `std::chrono::microseconds`.
template <class J>
struct LoadFromJSONImpl<std::chrono::microseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::microseconds& destination, const std::string& path) {
    int64_t value_as_int64;
    LoadFromJSONImpl<int64_t, J>::Load(source, value_as_int64, path);
    destination = std::chrono::microseconds(value_as_int64);
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

struct SavePrimitiveTypeIntoBinary {
  // `bool`.
  static void Save(std::ostream& ostream, const bool& value) {
    const BINARY_FORMAT_BOOL_TYPE b = static_cast<BINARY_FORMAT_BOOL_TYPE>(value);
    const size_t bytes_written =
        ostream.rdbuf()->sputn(reinterpret_cast<const char*>(&b), sizeof(BINARY_FORMAT_BOOL_TYPE));
    if (bytes_written != sizeof(BINARY_FORMAT_BOOL_TYPE)) {
      throw BinarySaveToStreamException(sizeof(BINARY_FORMAT_BOOL_TYPE), bytes_written);  // LCOV_EXCL_LINE
    }
  }

  // `std::string`.
  static void Save(std::ostream& ostream, const std::string& value) {
    SaveSizeIntoBinary(ostream, value.size());
    const size_t bytes_written = ostream.rdbuf()->sputn(value.data(), value.size());
    if (bytes_written != value.size()) {
      throw BinarySaveToStreamException(value.size(), bytes_written);  // LCOV_EXCL_LINE
    }
  }

  // `std::chrono::milliseconds`.
  static void Save(std::ostream& ostream, const std::chrono::milliseconds& value) {
    Save<int64_t>(ostream, value.count());
  }

  // `std::chrono::microseconds`.
  static void Save(std::ostream& ostream, const std::chrono::microseconds& value) {
    Save<int64_t>(ostream, value.count());
  }

  // All other primitive types accessed via raw pointer.
  template <typename T>
  static void Save(std::ostream& ostream, const T& value) {
    const size_t bytes_written =
        ostream.rdbuf()->sputn(reinterpret_cast<const char*>(std::addressof(value)), sizeof(T));
    if (bytes_written != sizeof(T)) {
      throw BinarySaveToStreamException(sizeof(T), bytes_written);  // LCOV_EXCL_LINE
    }
  }
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, ignore_h, ignore_fs, ignore_md) \
  template <>                                                                                         \
  struct SaveIntoBinaryImpl<cpp_type> {                                                               \
    static void Save(std::ostream& ostream, const cpp_type& value) {                                  \
      SavePrimitiveTypeIntoBinary::Save(ostream, value);                                              \
    }                                                                                                 \
  };
#include "../../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

}  // namespace save

namespace load {

struct LoadPrimitiveTypeFromBinary {
  // `bool`.
  static void Load(std::istream& istream, bool& destination) {
    BINARY_FORMAT_BOOL_TYPE b;
    const size_t bytes_read =
        istream.rdbuf()->sgetn(reinterpret_cast<char*>(&b), sizeof(BINARY_FORMAT_BOOL_TYPE));
    if (bytes_read != sizeof(BINARY_FORMAT_BOOL_TYPE)) {
      throw BinaryLoadFromStreamException(sizeof(BINARY_FORMAT_BOOL_TYPE), bytes_read);  // LCOV_EXCL_LINE
    }
    destination = b ? true : false;
  }

  // `std::string`.
  static void Load(std::istream& istream, std::string& destination) {
    BINARY_FORMAT_SIZE_TYPE size = LoadSizeFromBinary(istream);
    destination.resize(static_cast<size_t>(size));
    // Should be legitimate since c++11 requires internal buffer to be contiguous.
    const size_t bytes_read = istream.rdbuf()->sgetn(&destination[0], size);
    if (bytes_read != static_cast<size_t>(size)) {
      throw BinaryLoadFromStreamException(size, bytes_read);  // LCOV_EXCL_LINE
    }
  }

  // `std::chrono::milliseconds`.
  static void Load(std::istream& istream, std::chrono::milliseconds& destination) {
    int64_t value;
    Load<int64_t>(istream, value);
    destination = std::chrono::milliseconds(value);
  }

  // `std::chrono::microseconds`.
  static void Load(std::istream& istream, std::chrono::microseconds& destination) {
    int64_t value;
    Load<int64_t>(istream, value);
    destination = std::chrono::microseconds(value);
  }

  // All other primitive types accessed via raw pointer.
  template <typename T>
  static void Load(std::istream& istream, T& destination) {
    const size_t bytes_read =
        istream.rdbuf()->sgetn(reinterpret_cast<char*>(std::addressof(destination)), sizeof(T));
    if (bytes_read != sizeof(T)) {
      throw BinaryLoadFromStreamException(sizeof(T), bytes_read);  // LCOV_EXCL_LINE
    }
  }
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, ignore_h, ignore_fs, ignore_md) \
  template <>                                                                                         \
  struct LoadFromBinaryImpl<cpp_type> {                                                               \
    static void Load(std::istream& istream, cpp_type& destination) {                                  \
      LoadPrimitiveTypeFromBinary::Load(istream, destination);                                        \
    }                                                                                                 \
  };
#include "../../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PRIMITIVE_H
