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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_OPTIONAL_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_OPTIONAL_H

#include <type_traits>

#include "primitives.h"

#include "../../optional.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, Optional<T>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const Optional<T>& value) {
    if (Exists(value)) {
      Serialize(json_stringifier, Value(value));
    } else {
      // Current's default JSON parser would accept a missing field as well for no value,
      // but output it as `null` nonetheless, for clarity.
      json_stringifier.Current().SetNull();
    }
  }
};

template <typename T>
struct SerializeImpl<json::JSONStringifier<json::JSONFormat::Minimalistic>, Optional<T>> {
  static void DoSerialize(json::JSONStringifier<json::JSONFormat::Minimalistic>& json_stringifier,
                          const Optional<T>& value) {
    if (Exists(value)) {
      Serialize(json_stringifier, Value(value));
    } else {
      json_stringifier.MarkAsAbsentValue();
    }
  }
};

// In F#, `'T option` is an alias for `Optional<'T>`, which, in its turn, is a DU / Variant
// type with the signature `Optional<'T> = | Some 'T | None`.
// Thus, yeah, for a non-null `Optional` in F#, we need "Case" and "Fields".
template <typename T>
struct SerializeImpl<json::JSONStringifier<json::JSONFormat::NewtonsoftFSharp>, Optional<T>> {
  static void DoSerialize(json::JSONStringifier<json::JSONFormat::NewtonsoftFSharp>& json_stringifier,
                          const Optional<T>& value) {
    if (Exists(value)) {
      rapidjson::Value ultimate_destination;
      json_stringifier.Inner(&ultimate_destination, Value(value));
      rapidjson::Value array_of_one_element_containing_value;
      array_of_one_element_containing_value.SetArray();
      array_of_one_element_containing_value.PushBack(std::move(ultimate_destination.Move()),
                                                     json_stringifier.Allocator());
      json_stringifier.Current().SetObject();
      json_stringifier.Current().AddMember("Case", "Some", json_stringifier.Allocator());
      json_stringifier.Current().AddMember(
          "Fields", std::move(array_of_one_element_containing_value.Move()), json_stringifier.Allocator());
    } else {
      json_stringifier.MarkAsAbsentValue();
    }
  }
};

template <class JSON_FORMAT, typename T>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, Optional<T>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, Optional<T>& destination) {
    if (json_parser && !json_parser.Current().IsNull()) {
      destination = T();
      Deserialize(json_parser, Value(destination));
    } else {
      if (!json::JSONPatchMode<JSON_FORMAT>::value || json_parser) {
        // Nullify `destination` if at least one of two following conditions is true:
        // 1) The input JSON contains an explicit `null` (the `json_parser` check), OR
        // 2) The logic invoked is `ParseJSON`, not `PatchObjectWithJSON`.
        // Effectively, always nullify the destination in `ParseJSON`, mode, and, when in `PatchObjectWithJSON`
        // mode, take no action if the key is missing, yet treat the input `null` as explicit nullification.
        destination = nullptr;
      }
    }
  }
};

template <typename T>
struct DeserializeImpl<json::JSONParser<JSONFormat::NewtonsoftFSharp>, Optional<T>> {
  static void DoDeserialize(json::JSONParser<JSONFormat::NewtonsoftFSharp>& json_parser, Optional<T>& destination) {
    if (!json_parser || json_parser.Current().IsNull()) {
      destination = nullptr;
    } else {
      bool ok = false;
      if (json_parser.Current().IsObject() && json_parser.Current().HasMember("Case")) {
        const auto& case_field = json_parser.Current()["Case"];
        if (case_field.IsString()) {
          const char* case_field_value = case_field.GetString();
          if (!strcmp(case_field_value, "None")) {
            // Unnecessary, but to be safe. -- D.K.
            destination = nullptr;
            ok = true;
          } else if (!strcmp(case_field_value, "Some") && json_parser.Current().HasMember("Fields")) {
            auto& fields_field = json_parser.Current()["Fields"];
            if (fields_field.IsArray() && fields_field.Size() == 1u) {
              destination = T();
              json_parser.Inner(&fields_field[0u], Value(destination));
              ok = true;
            }
          }
        }
      }
      if (!ok) {
        CURRENT_THROW(
            JSONSchemaException("optional as `null` or `{\"Case\":\"Some\",\"Fields\":[value]}`", json_parser));
      }
    }
  }
};

namespace json {
template <typename T>
struct IsJSONSerializable<Optional<T>> {
  constexpr static bool value = IsJSONSerializable<T>::value;
};
}  // namespace json

}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_OPTIONAL_H
