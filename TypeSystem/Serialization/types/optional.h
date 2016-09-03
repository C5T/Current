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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_OPTIONAL_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_OPTIONAL_H

#include <type_traits>

#include "primitive.h"

#include "../../optional.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <typename T, class J>
struct SaveIntoJSONImpl<Optional<T>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T>& value) {
    if (Exists(value)) {
      SaveIntoJSONImpl<T, J>::Save(destination, allocator, Value(value));
      return true;
    } else {
      // Current's default JSON parser would accept a missing field as well for no value,
      // but output it as `null` nonetheless, for clarity.
      destination.SetNull();
      return true;
    }
  }
};

template <typename T>
struct SaveIntoJSONImpl<Optional<T>, JSONFormat::Minimalistic> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T>& value) {
    if (Exists(value)) {
      SaveIntoJSONImpl<T, JSONFormat::Minimalistic>::Save(destination, allocator, Value(value));
      return true;
    } else {
      return false;
    }
  }
};

// In F#, `'T option` is an alias for `Optional<'T>`, which, in its turn, is a DU / Variant
// type with the signature `Optional<'T> = | Some 'T | None`.
// Thus, yeah, for a non-null `Optional` in F#, we need "Case" and "Fields".
template <typename T>
struct SaveIntoJSONImpl<Optional<T>, JSONFormat::NewtonsoftFSharp> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T>& value) {
    if (Exists(value)) {
      rapidjson::Value ultimate_destination;
      SaveIntoJSONImpl<T, JSONFormat::Minimalistic>::Save(ultimate_destination, allocator, Value(value));
      rapidjson::Value array_of_one_element_containing_value;
      array_of_one_element_containing_value.SetArray();
      array_of_one_element_containing_value.PushBack(ultimate_destination.Move(), allocator);
      destination.SetObject();
      destination.AddMember("Case", "Some", allocator);
      destination.AddMember("Fields", array_of_one_element_containing_value, allocator);
      return true;
    } else {
      return false;
    }
  }
};

}  // namespace save

namespace load {

template <typename T, class J>
struct LoadFromJSONImpl<Optional<T>, J> {
  static void Load(rapidjson::Value* source, Optional<T>& destination, const std::string& path) {
    if (source && !source->IsNull()) {
      destination = T();
      LoadFromJSONImpl<T, J>::Load(source, Value(destination), path);
    } else {
      if (!JSONPatchMode<J>::value || source) {
        // Nullify `destination` if at least one of two following conditions is true:
        // 1) The input JSON contains an explicit `null` (the `source` check), OR
        // 2) The logic invoked is `ParseJSON`, not `PatchJSON`.
        // Effectively, always nullify the destination in `ParseJSON`, mode, and when in `PatchJSON` mode,
        // take no action if the key is plain missing, yet treat explicit `null` as explicit nullification.
        destination = nullptr;
      }
    }
  }
};

template <typename T>
struct LoadFromJSONImpl<Optional<T>, JSONFormat::NewtonsoftFSharp> {
  static void Load(rapidjson::Value* source, Optional<T>& destination, const std::string& path) {
    if (!source || source->IsNull()) {
      destination = nullptr;
    } else {
      bool ok = false;
      if (source->IsObject() && source->HasMember("Case")) {
        const auto& case_field = (*source)["Case"];
        if (case_field.IsString()) {
          const char* case_field_value = case_field.GetString();
          if (!strcmp(case_field_value, "None")) {
            // Unnecessary, but to be safe. -- D.K.
            destination = nullptr;
            ok = true;
          } else if (!strcmp(case_field_value, "Some") && source->HasMember("Fields")) {
            auto& fields_field = (*source)["Fields"];
            if (fields_field.IsArray() && fields_field.Size() == 1u) {
              destination = T();
              LoadFromJSONImpl<T, JSONFormat::NewtonsoftFSharp>::Load(
                  &fields_field[0u], Value(destination), path);
              ok = true;
            }
          }
        }
      }
      if (!ok) {
        throw JSONSchemaException(
            "optional as `null` or `{\"Case\":\"Some\",\"Fields\":[value]}`", source, path);
      }
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <typename T>
struct SaveIntoBinaryImpl<Optional<T>> {
  static void Save(std::ostream& ostream, const Optional<T>& value) {
    const bool exists = Exists(value);
    SaveIntoBinaryImpl<bool>::Save(ostream, exists);
    if (exists) {
      SaveIntoBinaryImpl<T>::Save(ostream, Value(value));
    }
  }
};

}  // namespace save

namespace load {

template <typename T>
struct LoadFromBinaryImpl<Optional<T>> {
  static void Load(std::istream& istream, Optional<T>& destination) {
    bool exists;
    LoadFromBinaryImpl<bool>::Load(istream, exists);
    if (exists) {
      destination = T();
      LoadFromBinaryImpl<T>::Load(istream, Value(destination));
    } else {
      destination = nullptr;
    }
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_OPTIONAL_H
