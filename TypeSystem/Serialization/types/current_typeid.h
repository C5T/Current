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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_TYPEID_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_TYPEID_H

#include "primitive.h"

#include "../../Reflection/types.h"

#include "../../../Bricks/strings/util.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <JSONFormat J>
struct SaveIntoJSONImpl<reflection::TypeID, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   reflection::TypeID value) {
    destination.SetString("T" + strings::ToString(static_cast<uint64_t>(value)), allocator);
    return true;
  }
};

}  // namespace save

namespace load {

template <JSONFormat J>
struct LoadFromJSONImpl<reflection::TypeID, J> {
  static void Load(rapidjson::Value* source, reflection::TypeID& destination, const std::string& path) {
    if (source && source->IsString() && *source->GetString() == 'T') {
      destination = static_cast<reflection::TypeID>(strings::FromString<uint64_t>(source->GetString() + 1));
    } else {
      throw JSONSchemaException("TypeID", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <>
struct SaveIntoBinaryImpl<reflection::TypeID> {
  static void Save(std::ostream& ostream, reflection::TypeID value) {
    SaveIntoBinaryImpl<uint64_t>::Save(ostream, static_cast<uint64_t>(value));
  }
};

}  // namespace save

namespace load {

template <>
struct LoadFromBinaryImpl<reflection::TypeID> {
  static void Load(std::istream& istream, reflection::TypeID& destination) {
    uint64_t value;
    LoadFromBinaryImpl<uint64_t>::Load(istream, value);
    destination = static_cast<reflection::TypeID>(value);
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_TYPEID_H
