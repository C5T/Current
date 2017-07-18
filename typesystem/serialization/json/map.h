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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_MAP_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_MAP_H

#include <type_traits>
#include <map>

#include "json.h"

#include "../../../bricks/template/enable_if.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename TK, typename TV, typename TC, typename TA>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::map<TK, TV, TC, TA>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const std::map<TK, TV, TC, TA>& value) {
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

template <class JSON_FORMAT, typename TV, typename TC, typename TA>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::map<std::string, TV, TC, TA>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier,
                          const std::map<std::string, TV, TC, TA>& value) {
    json_stringifier.Current().SetObject();
    for (const auto& element : value) {
      rapidjson::Value populated_value;
      json_stringifier.Inner(&populated_value, element.second);
      json_stringifier.Current().AddMember(
          rapidjson::StringRef(element.first), std::move(populated_value.Move()), json_stringifier.Allocator());
    }
  }
};

template <class JSON_FORMAT, typename TK, typename TV, typename TC, typename TA, class J>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::map<TK, TV, TC, TA>, J> {
  template <typename K = TK>
  static std::enable_if_t<std::is_same<std::string, K>::value> DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser,
                                                                             std::map<TK, TV, TC, TA>& destination) {
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
      throw JSONSchemaException("map as object", json_parser);  // LCOV_EXCL_LINE
    }
  }

  template <typename K = TK>
  static std::enable_if_t<!std::is_same<std::string, K>::value> DoDeserialize(
      json::JSONParser<JSON_FORMAT>& json_parser, std::map<TK, TV, TC, TA>& destination) {
    if (json_parser && json_parser.Current().IsArray()) {
      destination.clear();
      for (rapidjson::Value::ValueIterator cit = json_parser.Current().Begin(); cit != json_parser.Current().End();
           ++cit) {
        if (!cit->IsArray()) {
          throw JSONSchemaException("map entry as array", json_parser);  // LCOV_EXCL_LINE
        }
        if (cit->Size() != 2u) {
          throw JSONSchemaException("map entry as array of two elements", json_parser);  // LCOV_EXCL_LINE
        }
        TK k;
        TV v;
        json_parser.Inner(&(*cit)[static_cast<rapidjson::SizeType>(0)], k);
        json_parser.Inner(&(*cit)[static_cast<rapidjson::SizeType>(1)], v);
        destination.emplace(std::move(k), std::move(v));
      }
    } else if (!json::JSONPatchMode<J>::value || (json_parser && !json_parser.Current().IsArray())) {
      throw JSONSchemaException("map as array", json_parser);  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <typename K, typename V, typename CMP, typename ALLOC>
struct IsJSONSerializable<std::map<K, V, CMP, ALLOC>> {
  constexpr static bool value = IsJSONSerializable<K>::value && IsJSONSerializable<V>::value;
};
}  // namespace json

}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_MAP_H
