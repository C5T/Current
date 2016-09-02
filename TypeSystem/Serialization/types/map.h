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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_MAP_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_MAP_H

#include <type_traits>
#include <map>

#include "../base.h"

#include "../../../Bricks/template/enable_if.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <typename TK, typename TV, typename TC, typename TA, class J>
struct SaveIntoJSONImpl<std::map<TK, TV, TC, TA>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::map<TK, TV, TC, TA>& value) {
    destination.SetArray();
    for (const auto& element : value) {
      rapidjson::Value key_value_as_array;
      key_value_as_array.SetArray();
      rapidjson::Value populated_key;
      rapidjson::Value populated_value;
      SaveIntoJSONImpl<TK, J>::Save(populated_key, allocator, element.first);
      SaveIntoJSONImpl<TV, J>::Save(populated_value, allocator, element.second);
      key_value_as_array.PushBack(populated_key, allocator);
      key_value_as_array.PushBack(populated_value, allocator);
      destination.PushBack(key_value_as_array, allocator);
    }
    return true;
  }
};

template <typename TV, typename TC, typename TA, class J>
struct SaveIntoJSONImpl<std::map<std::string, TV, TC, TA>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::map<std::string, TV, TC, TA>& value) {
    destination.SetObject();
    for (const auto& element : value) {
      rapidjson::Value populated_value;
      SaveIntoJSONImpl<TV, J>::Save(populated_value, allocator, element.second);
      destination.AddMember(rapidjson::StringRef(element.first), populated_value, allocator);
    }
    return true;
  }
};

}  // namespace save

namespace load {

template <typename TK, typename TV, typename TC, typename TA, class J>
struct LoadFromJSONImpl<std::map<TK, TV, TC, TA>, J> {
  template <typename K = TK>
  static ENABLE_IF<std::is_same<std::string, K>::value> Load(rapidjson::Value* source,
                                                             std::map<TK, TV, TC, TA>& destination,
                                                             const std::string& path) {
    if (source && source->IsObject()) {
      destination.clear();
      TK k;
      TV v;
      for (rapidjson::Value::MemberIterator cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
        LoadFromJSONImpl<TK, J>::Load(&cit->name, k, path);
        LoadFromJSONImpl<TV, J>::Load(&cit->value, v, path);
        destination.emplace(k, v);
      }
    } else if (!JSONPatchMode<J>::value || (source && !source->IsObject())) {
      throw JSONSchemaException("map as object", source, path);  // LCOV_EXCL_LINE
    }
  }

  template <typename K = TK>
  static ENABLE_IF<!std::is_same<std::string, K>::value> Load(rapidjson::Value* source,
                                                              std::map<TK, TV, TC, TA>& destination,
                                                              const std::string& path) {
    if (source && source->IsArray()) {
      destination.clear();
      for (rapidjson::Value::ValueIterator cit = source->Begin(); cit != source->End(); ++cit) {
        if (!cit->IsArray()) {
          throw JSONSchemaException("map entry as array", source, path);  // LCOV_EXCL_LINE
        }
        if (cit->Size() != 2u) {
          throw JSONSchemaException("map entry as array of two elements", source, path);  // LCOV_EXCL_LINE
        }
        TK k;
        TV v;
        LoadFromJSONImpl<TK, J>::Load(&(*cit)[static_cast<rapidjson::SizeType>(0)], k, path);
        LoadFromJSONImpl<TV, J>::Load(&(*cit)[static_cast<rapidjson::SizeType>(1)], v, path);
        destination.emplace(k, v);
      }
    } else if (!JSONPatchMode<J>::value || (source && !source->IsArray())) {
      throw JSONSchemaException("map as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <typename TK, typename TV, typename TC, typename TA>
struct SaveIntoBinaryImpl<std::map<TK, TV, TC, TA>> {
  static void Save(std::ostream& ostream, const std::map<TK, TV, TC, TA>& value) {
    SaveSizeIntoBinary(ostream, value.size());
    for (const auto& element : value) {
      SaveIntoBinaryImpl<std::pair<TK, TV>>::Save(ostream, element);
    }
  }
};

}  // namespace save

namespace load {

template <typename TK, typename TV, typename TC, typename TA>
struct LoadFromBinaryImpl<std::map<TK, TV, TC, TA>> {
  static void Load(std::istream& istream, std::map<TK, TV, TC, TA>& destination) {
    destination.clear();
    BINARY_FORMAT_SIZE_TYPE size = LoadSizeFromBinary(istream);
    TK k;
    TV v;
    for (size_t i = 0; i < static_cast<size_t>(size); ++i) {
      LoadFromBinaryImpl<TK>::Load(istream, k);
      LoadFromBinaryImpl<TV>::Load(istream, v);
      destination.emplace(k, v);
    }
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_MAP_H
