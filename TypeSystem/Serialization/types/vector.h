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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H

#include <vector>

#include "../json.h"

namespace current {
namespace serialization {
namespace json {

namespace save {

template <typename TT, typename TA, JSONFormat J>
struct SaveIntoJSONImpl<std::vector<TT, TA>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::vector<TT, TA>& value) {
    destination.SetArray();
    rapidjson::Value element_to_push;
    for (const auto& element : value) {
      SaveIntoJSONImpl<TT, J>::Save(element_to_push, allocator, element);
      destination.PushBack(element_to_push, allocator);
    }
    return true;
  }
};

}  // namespace save

namespace load {

template <typename T, typename A, JSONFormat J>
struct LoadFromJSONImpl<std::vector<T, A>, J> {
  static void Load(rapidjson::Value* source, std::vector<T, A>& destination, const std::string& path) {
    if (source && source->IsArray()) {
      const size_t size = source->Size();
      destination.resize(size);
      for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(size); ++i) {
        LoadFromJSONImpl<T, J>::Load(&((*source)[i]), destination[i], path + '[' + std::to_string(i) + ']');
      }
    } else {
      throw JSONSchemaException("array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load

}  // namespace json
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VECTOR_H


