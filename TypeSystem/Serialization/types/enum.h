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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_ENUM_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_ENUM_H

#include <type_traits>

#include "primitive.h"

#include "../../../Bricks/template/enable_if.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <typename T, JSONFormat J>
struct SaveIntoJSONImpl<T, J, ENABLE_IF<std::is_enum<T>::value>> {
  static bool Save(rapidjson::Value& destination, rapidjson::Document::AllocatorType&, const T& value) {
    AssignToRapidJSONValue(destination, static_cast<typename std::underlying_type<T>::type>(value));
    return true;
  }
};

}  // namespace save

namespace load {

template <typename T, JSONFormat J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<std::is_enum<T>::value>> {
  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      if (std::numeric_limits<typename std::underlying_type<T>::type>::is_signed) {
        destination = static_cast<T>(source->GetInt64());
      } else {
        destination = static_cast<T>(source->GetUint64());
      }
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <typename T>
struct SaveIntoBinaryImpl<T, ENABLE_IF<std::is_enum<T>::value>> {
  static void Save(std::ostream& ostream, const T& value) {
    SavePrimitiveTypeIntoBinary::Save(ostream, static_cast<typename std::underlying_type<T>::type>(value));
  }
};

}  // namespace save

namespace load {

template <typename T>
struct LoadFromBinaryImpl<T, ENABLE_IF<std::is_enum<T>::value>> {
  static void Load(std::istream& istream, T& destination) {
    using T_UNDERLYING = typename std::underlying_type<T>::type;
    const size_t bytes_read =
        istream.rdbuf()->sgetn(reinterpret_cast<char*>(std::addressof(destination)), sizeof(T_UNDERLYING));
    if (bytes_read != sizeof(T_UNDERLYING)) {
      throw BinaryLoadFromStreamException(sizeof(T_UNDERLYING), bytes_read);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_ENUM_H
