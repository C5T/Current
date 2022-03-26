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

// Shamelessly copy-pasted `map.h`. -- D.K.

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_UNORDERED_MAP_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_UNORDERED_MAP_H

#include <type_traits>
#include <unordered_map>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename TK, typename TV, class HASH, class EQ, class ALLOCATOR>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::unordered_map<TK, TV, HASH, EQ, ALLOCATOR>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier,
                          const std::unordered_map<TK, TV, HASH, EQ, ALLOCATOR>& value) {
    json_stringifier.Current().SetArray();
    for (const auto& element : value) {
      rapidjson::Value key_value_as_array;
      key_value_as_array.SetArray();
      rapidjson::Value populated_key;
      rapidjson::Value populated_value;
      json_stringifier.Inner(&populated_key, element.first);
      json_stringifier.Inner(&populated_value, element.second);
      key_value_as_array.PushBack(std::move(populated_key.Move()), json_stringifier.Allocator());
      key_value_as_array.PushBack(std::move(populated_value.Move()), json_stringifier.Allocator());
      json_stringifier.Current().PushBack(std::move(key_value_as_array.Move()), json_stringifier.Allocator());
    }
  }
};

template <class JSON_FORMAT, typename TV, class HASH, class EQ, class ALLOCATOR>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::unordered_map<std::string, TV, HASH, EQ, ALLOCATOR>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier,
                          const std::unordered_map<std::string, TV, HASH, EQ, ALLOCATOR>& value) {
    json_stringifier.Current().SetObject();
    for (const auto& element : value) {
      rapidjson::Value populated_value;
      json_stringifier.Inner(&populated_value, element.second);
      json_stringifier.Current().AddMember(
          rapidjson::StringRef(element.first), std::move(populated_value.Move()), json_stringifier.Allocator());
    }
  }
};

template <class JSON_FORMAT, typename TK, typename TV, class HASH, class EQ, class ALLOCATOR, class J>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::unordered_map<TK, TV, HASH, EQ, ALLOCATOR>, J> {
  template <typename K = TK>
  static std::enable_if_t<std::is_same_v<std::string, K>> DoDeserialize(
      json::JSONParser<JSON_FORMAT>& json_parser, std::unordered_map<TK, TV, HASH, EQ, ALLOCATOR>& destination) {
    if (json_parser && json_parser.Current().IsObject()) {
      destination.clear();
      TK k;
      TV v;
      for (rapidjson::Value::MemberIterator cit = json_parser.Current().MemberBegin();
           cit != json_parser.Current().MemberEnd();
           ++cit) {
        json_parser.Inner(&cit->name, k);
        json_parser.Inner(&cit->value, v);
        destination.emplace(std::move(k), std::move(v));
      }
    } else if (!json::JSONPatchMode<J>::value || (json_parser && !json_parser.Current().IsObject())) {
      CURRENT_THROW(JSONSchemaException("[unordered_]map as object", json_parser));  // LCOV_EXCL_LINE
    }
  }

  template <typename K = TK>
  static std::enable_if_t<!std::is_same_v<std::string, K>> DoDeserialize(
      json::JSONParser<JSON_FORMAT>& json_parser, std::unordered_map<TK, TV, HASH, EQ, ALLOCATOR>& destination) {
    if (json_parser && json_parser.Current().IsArray()) {
      destination.clear();
      for (rapidjson::Value::ValueIterator cit = json_parser.Current().Begin(); cit != json_parser.Current().End();
           ++cit) {
        if (!cit->IsArray()) {
          CURRENT_THROW(JSONSchemaException("[unordered_]map entry as array", json_parser));  // LCOV_EXCL_LINE
        }
        if (cit->Size() != 2u) {
          // LCOV_EXCL_START
          CURRENT_THROW(JSONSchemaException("[unordered_]map entry as array of two elements", json_parser));
          // LCOV_EXCL_STOP
        }
        TK k;
        TV v;
        json_parser.Inner(&(*cit)[static_cast<rapidjson::SizeType>(0)], k);
        json_parser.Inner(&(*cit)[static_cast<rapidjson::SizeType>(1)], v);
        destination.emplace(std::move(k), std::move(v));
      }
    } else if (!json::JSONPatchMode<J>::value || (json_parser && !json_parser.Current().IsArray())) {
      CURRENT_THROW(JSONSchemaException("[unordered_]map as array", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <typename K, typename V, typename HASH, typename ALLOC>
struct IsJSONSerializable<std::unordered_map<K, V, HASH, ALLOC>> {
  constexpr static bool value = IsJSONSerializable<K>::value && IsJSONSerializable<V>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_UNORDERED_MAP_H
