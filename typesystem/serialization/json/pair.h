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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PAIR_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PAIR_H

#include <utility>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename TF, typename TS>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::pair<TF, TS>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const std::pair<TF, TS>& value) {
    rapidjson::Value first_value;
    rapidjson::Value second_value;
    json_stringifier.Inner(&first_value, value.first);
    json_stringifier.Inner(&second_value, value.second);
    json_stringifier.Current().SetArray();
    json_stringifier.Current().PushBack(std::move(first_value.Move()), json_stringifier.Allocator());
    json_stringifier.Current().PushBack(std::move(second_value.Move()), json_stringifier.Allocator());
  }
};

template <typename TF, typename TS>
struct SerializeImpl<json::JSONStringifier<json::JSONFormat::NewtonsoftFSharp>, std::pair<TF, TS>> {
  static void DoSerialize(json::JSONStringifier<json::JSONFormat::NewtonsoftFSharp>& json_stringifier,
                          const std::pair<TF, TS>& value) {
    rapidjson::Value first_value;
    rapidjson::Value second_value;
    json_stringifier.Inner(&first_value, value.first);
    json_stringifier.Inner(&second_value, value.second);
    json_stringifier.Current().SetObject();
    json_stringifier.Current().AddMember("Item1", std::move(first_value.Move()), json_stringifier.Allocator());
    json_stringifier.Current().AddMember("Item2", std::move(second_value.Move()), json_stringifier.Allocator());
  }
};

template <class JSON_FORMAT, typename TF, typename TS>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::pair<TF, TS>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::pair<TF, TS>& destination) {
    if (json_parser && json_parser.Current().IsArray() && json_parser.Current().Size() == 2u) {
      json_parser.Inner(&json_parser.Current()[static_cast<rapidjson::SizeType>(0)], destination.first);
      json_parser.Inner(&json_parser.Current()[static_cast<rapidjson::SizeType>(1)], destination.second);
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value ||
               (json_parser && !(json_parser.Current().IsArray() && json_parser.Current().Size() == 2u))) {
      CURRENT_THROW(JSONSchemaException("pair as array", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

template <typename TF, typename TS>
struct DeserializeImpl<json::JSONParser<JSONFormat::NewtonsoftFSharp>, std::pair<TF, TS>> {
  static void DoDeserialize(json::JSONParser<JSONFormat::NewtonsoftFSharp>& json_parser,
                            std::pair<TF, TS>& destination) {
    if (json_parser && json_parser.Current().IsObject() && json_parser.Current().HasMember("Item1") &&
        json_parser.Current().HasMember("Item2")) {
      json_parser.Inner(&json_parser.Current()["Item1"], destination.first);
      json_parser.Inner(&json_parser.Current()["Item2"], destination.second);
    } else {
      CURRENT_THROW(JSONSchemaException("pair as an object of {Item1,Item2}", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <typename F, typename S>
struct IsJSONSerializable<std::pair<F, S>> {
  constexpr static bool value = IsJSONSerializable<F>::value && IsJSONSerializable<S>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_PAIR_H
