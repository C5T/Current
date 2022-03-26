/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// NOTE(dkorolev): Shamelessly copy-pasted `vector.h`.

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_SET_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_SET_H

#include <set>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T, class EQ, class ALLOCATOR>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::set<T, EQ, ALLOCATOR>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier,
                          const std::set<T, EQ, ALLOCATOR>& value) {
    json_stringifier.Current().SetArray();
    for (const auto& element : value) {
      rapidjson::Value element_to_push;
      json_stringifier.Inner(&element_to_push, element);
      json_stringifier.Current().PushBack(std::move(element_to_push.Move()), json_stringifier.Allocator());
    }
  }
};

template <class JSON_FORMAT, typename T, class EQ, class ALLOCATOR>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::set<T, EQ, ALLOCATOR>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::set<T, EQ, ALLOCATOR>& destination) {
    destination.clear();
    if (json_parser && json_parser.Current().IsArray()) {
      const size_t size = json_parser.Current().Size();
      for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(size); ++i) {
        T element;
        json_parser.Inner(&json_parser.Current()[i], element, "[", static_cast<int>(i), "]");
        destination.insert(std::move(element));
      }
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsArray())) {
      CURRENT_THROW(JSONSchemaException("set as array", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <typename T, typename CMP, typename ALLOC>
struct IsJSONSerializable<std::set<T, CMP, ALLOC>> {
  constexpr static bool value = IsJSONSerializable<T>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_SET_H
