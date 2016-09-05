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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ENUM_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ENUM_H

#include <type_traits>

#include "primitives.h"

#include "../../../Bricks/template/enable_if.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, T, std::enable_if_t<std::is_enum<T>::value>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const T enum_value) {
    json_stringifier = static_cast<typename std::underlying_type<T>::type>(enum_value);
  }
};

namespace json {
namespace load {

template <typename T, class J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<std::is_enum<T>::value>> {
  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    // TODO(dkorolev): This `IsNumber` vs. `Get[U]Int64` part is scary.
    if (source && source->IsNumber()) {
      if (std::numeric_limits<typename std::underlying_type<T>::type>::is_signed) {
        if (source->IsInt64()) {
          destination = static_cast<T>(source->GetInt64());
        } else {
          throw JSONSchemaException("enum as unsigned integer", source, path);  // LCOV_EXCL_LINE
        }
      } else {
        if (source->IsUint64()) {
          destination = static_cast<T>(source->GetUint64());
        } else {
          throw JSONSchemaException("enum as signed integer", source, path);  // LCOV_EXCL_LINE
        }
      }
    } else if (!JSONPatchMode<J>::value || (source && !source->IsNumber())) {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace current::serialization::json::load
}  // namespace current::serialization::json
}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ENUM_H
