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

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, T, std::enable_if_t<std::is_enum_v<T>>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const T enum_value) {
    json_stringifier = static_cast<typename std::underlying_type<T>::type>(enum_value);
  }
};

template <class JSON_FORMAT, typename T>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, T, std::enable_if_t<std::is_enum_v<T>>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, T& destination) {
    if (json_parser && json_parser.Current().IsNumber()) {
      if (std::numeric_limits<typename std::underlying_type<T>::type>::is_signed) {
        if (json_parser.Current().IsInt64()) {
          destination = static_cast<T>(json_parser.Current().GetInt64());
        } else {
          CURRENT_THROW(JSONSchemaException("enum as unsigned integer", json_parser));  // LCOV_EXCL_LINE
        }
      } else {
        if (json_parser.Current().IsUint64()) {
          destination = static_cast<T>(json_parser.Current().GetUint64());
        } else {
          CURRENT_THROW(JSONSchemaException("enum as signed integer", json_parser));  // LCOV_EXCL_LINE
        }
      }
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsNumber())) {
      CURRENT_THROW(JSONSchemaException("number", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ENUM_H
