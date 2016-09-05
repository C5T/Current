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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VECTOR_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VECTOR_H

#include <vector>

#include "json.h"

namespace current {
namespace serialization {

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, std::vector<T>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const std::vector<T>& value) {
    json_stringifier.Current().SetArray();
    for (const auto& element : value) {
      rapidjson::Value element_to_push;
      json_stringifier.Inner(&element_to_push, element);
      json_stringifier.Current().PushBack(std::move(element_to_push.Move()), json_stringifier.Allocator());
    }
  }
};

namespace json {
namespace load {

template <typename TT, typename TA, class J>
struct LoadFromJSONImpl<std::vector<TT, TA>, J> {
  static void Load(rapidjson::Value* source, std::vector<TT, TA>& destination, const std::string& path) {
    if (source && source->IsArray()) {
      const size_t size = source->Size();
      destination.resize(size);
      for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(size); ++i) {
        LoadFromJSONImpl<TT, J>::Load(&((*source)[i]), destination[i], path + '[' + std::to_string(i) + ']');
      }
    } else if (!JSONPatchMode<J>::value || (source && !source->IsArray())) {
      throw JSONSchemaException("array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace current::serialization::json::load
}  // namespace current::serialization::json
}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VECTOR_H
