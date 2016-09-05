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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TYPEID_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TYPEID_H

#include "primitives.h"

#include "../../Reflection/types.h"

#include "../../../Bricks/strings/util.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, reflection::TypeID> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, reflection::TypeID value) {
    json_stringifier.Current().SetString("T" + current::ToString(value), json_stringifier.Allocator());
  }
};

namespace json {
namespace load {

template <class J>
struct LoadFromJSONImpl<reflection::TypeID, J> {
  static void Load(rapidjson::Value* source, reflection::TypeID& destination, const std::string& path) {
    if (source && source->IsString() && *source->GetString() == 'T') {
      destination = static_cast<reflection::TypeID>(current::FromString<uint64_t>(source->GetString() + 1));
    } else if (!JSONPatchMode<J>::value || (source && !(source->IsString() && *source->GetString() == 'T'))) {
      throw JSONSchemaException("TypeID", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace current::serialization::json::load
}  // namespace current::serialization::json
}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_TYPEID_H
