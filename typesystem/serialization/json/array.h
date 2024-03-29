/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2023 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ARRAY_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ARRAY_H

#include <array>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T, size_t N>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::array<T, N>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const std::array<T, N>& value) {
    json_stringifier.Current().SetArray();
    for (const auto& element : value) {
      rapidjson::Value element_to_push;
      json_stringifier.Inner(&element_to_push, element);
      json_stringifier.Current().PushBack(std::move(element_to_push.Move()), json_stringifier.Allocator());
    }
  }
};

template <class JSON_FORMAT, typename T, size_t N>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::array<T, N>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::array<T, N>& destination) {
    if (json_parser && json_parser.Current().IsArray()) {
      for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(N); ++i) {
        json_parser.Inner(&json_parser.Current()[i], destination[i], "[", static_cast<int>(i), "]");
      }
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsArray())) {
      CURRENT_THROW(JSONSchemaException("array", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <typename T, size_t N>
struct IsJSONSerializable<std::array<T, N>> {
  constexpr static bool value = IsJSONSerializable<T>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_ARRAY_H
