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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PAIR_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PAIR_H

#include <utility>

#include "../base.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <typename TF, typename TS, class J>
struct SaveIntoJSONImpl<std::pair<TF, TS>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::pair<TF, TS>& value) {
    destination.SetArray();
    rapidjson::Value first_value;
    rapidjson::Value second_value;
    SaveIntoJSONImpl<TF, J>::Save(first_value, allocator, value.first);
    SaveIntoJSONImpl<TS, J>::Save(second_value, allocator, value.second);
    destination.PushBack(first_value, allocator);
    destination.PushBack(second_value, allocator);
    return true;
  }
};

template <typename TF, typename TS>
struct SaveIntoJSONImpl<std::pair<TF, TS>, JSONFormat::NewtonsoftFSharp> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::pair<TF, TS>& value) {
    destination.SetObject();
    rapidjson::Value first_value;
    rapidjson::Value second_value;
    SaveIntoJSONImpl<TF, JSONFormat::NewtonsoftFSharp>::Save(first_value, allocator, value.first);
    SaveIntoJSONImpl<TS, JSONFormat::NewtonsoftFSharp>::Save(second_value, allocator, value.second);
    destination.AddMember("Item1", first_value, allocator);
    destination.AddMember("Item2", second_value, allocator);
    return true;
  }
};

}  // namespace save

namespace load {

template <typename TF, typename TS, class J>
struct LoadFromJSONImpl<std::pair<TF, TS>, J> {
  static void Load(rapidjson::Value* source, std::pair<TF, TS>& destination, const std::string& path) {
    if (source && source->IsArray() && source->Size() == 2u) {
      LoadFromJSONImpl<TF, J>::Load(&((*source)[static_cast<rapidjson::SizeType>(0)]), destination.first, path);
      LoadFromJSONImpl<TS, J>::Load(
          &((*source)[static_cast<rapidjson::SizeType>(1)]), destination.second, path);
    } else if (!JSONPatchMode<J>::value || (source && !(source->IsArray() && source->Size() == 2u))) {
      throw JSONSchemaException("pair as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename TF, typename TS>
struct LoadFromJSONImpl<std::pair<TF, TS>, JSONFormat::NewtonsoftFSharp> {
  static void Load(rapidjson::Value* source, std::pair<TF, TS>& destination, const std::string& path) {
    if (source && source->IsObject() && source->HasMember("Item1") && source->HasMember("Item2")) {
      LoadFromJSONImpl<TF, JSONFormat::NewtonsoftFSharp>::Load(&((*source)["Item1"]), destination.first, path);
      LoadFromJSONImpl<TS, JSONFormat::NewtonsoftFSharp>::Load(&((*source)["Item2"]), destination.second, path);
    } else {
      throw JSONSchemaException("pair as an object of {Item1,Item2}", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <typename TF, typename TS>
struct SaveIntoBinaryImpl<std::pair<TF, TS>> {
  static void Save(std::ostream& ostream, const std::pair<TF, TS>& value) {
    SaveIntoBinaryImpl<TF>::Save(ostream, value.first);
    SaveIntoBinaryImpl<TS>::Save(ostream, value.second);
  }
};

}  // namespace save

namespace load {

template <typename TF, typename TS>
struct LoadFromBinaryImpl<std::pair<TF, TS>> {
  static void Load(std::istream& istream, std::pair<TF, TS>& destination) {
    LoadFromBinaryImpl<TF>::Load(istream, destination.first);
    LoadFromBinaryImpl<TS>::Load(istream, destination.second);
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_PAIR_H
