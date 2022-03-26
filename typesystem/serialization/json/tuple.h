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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TUPLE_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TUPLE_H

#include <tuple>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, class TUPLE, int I, int N>
struct SerializeTupleImpl {
  static void DoIt(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const TUPLE& value) {
    rapidjson::Value element_to_push;
    json_stringifier.Inner(&element_to_push, std::get<I>(value));
    json_stringifier.Current().PushBack(std::move(element_to_push.Move()), json_stringifier.Allocator());
    SerializeTupleImpl<JSON_FORMAT, TUPLE, I + 1, N>::DoIt(json_stringifier, value);
  }
};

template <class JSON_FORMAT, class TUPLE, int N>
struct SerializeTupleImpl<JSON_FORMAT, TUPLE, N, N> {
  static void DoIt(json::JSONStringifier<JSON_FORMAT>&, const TUPLE&) {}
};

template <class JSON_FORMAT, typename... TS>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::tuple<TS...>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const std::tuple<TS...>& value) {
    json_stringifier.Current().SetArray();
    SerializeTupleImpl<JSON_FORMAT, std::tuple<TS...>, 0, sizeof...(TS)>::DoIt(json_stringifier, value);
  }
};

template <class JSON_FORMAT, class TUPLE, int I, int N>
struct DeserializeTupleImpl {
  static void DoIt(json::JSONParser<JSON_FORMAT>& json_parser, TUPLE& destination) {
    json_parser.Inner(&json_parser.Current()[I], std::get<I>(destination), "[", I, "]");
    DeserializeTupleImpl<JSON_FORMAT, TUPLE, I + 1, N>::DoIt(json_parser, destination);
  }
};

template <class JSON_FORMAT, class TUPLE, int N>
struct DeserializeTupleImpl<JSON_FORMAT, TUPLE, N, N> {
  static void DoIt(json::JSONParser<JSON_FORMAT>&, TUPLE&) {}
};

template <class JSON_FORMAT, typename... TS>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, std::tuple<TS...>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, std::tuple<TS...>& destination) {
    if (json_parser && json_parser.Current().IsArray()) {
      constexpr size_t N = sizeof...(TS);
      if (N != json_parser.Current().Size()) {
        // TODO(dkorolev): The error message to include both desired and actual size?
        CURRENT_THROW(JSONSchemaException("bad array size for tuple", json_parser));  // LCOV_EXCL_LINE
      }
      DeserializeTupleImpl<JSON_FORMAT, std::tuple<TS...>, 0, sizeof...(TS)>::DoIt(json_parser, destination);
    } else if (!json::JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsArray())) {
      CURRENT_THROW(JSONSchemaException("array", json_parser));  // LCOV_EXCL_LINE
    }
  }
};

namespace json {
template <>
struct IsJSONSerializable<std::tuple<>> {
  constexpr static bool value = true;
};
template <typename T, typename... TS>
struct IsJSONSerializable<std::tuple<T, TS...>> {
  constexpr static bool value = IsJSONSerializable<T>::value && IsJSONSerializable<std::tuple<TS...>>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TUPLE_H
