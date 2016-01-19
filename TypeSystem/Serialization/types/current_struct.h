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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_STRUCT_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_STRUCT_H

#include <type_traits>

#include "../json.h"

#include "../../struct.h"
#include "../../types.h"

#include "../../../Bricks/template/enable_if.h"

namespace current {
namespace serialization {
namespace json {

namespace save {

template <typename T, JSONFormat J>
struct SaveIntoJSONImpl<T, J, ENABLE_IF<IS_CURRENT_STRUCT(T)>> {
  struct SaveFieldVisitor {
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;

    SaveFieldVisitor(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    // IMPORTANT: Pass in `const char* name`, as `const std::string& name`
    // would fail memory-allocation-wise due to over-smartness of RapidJSON.
    template <typename U>
    void operator()(const char* name, const U& source) const {
      rapidjson::Value placeholder;
      if (SaveIntoJSONImpl<U, J>::Save(placeholder, allocator_, source)) {
        destination_.AddMember(rapidjson::StringRef(name), placeholder, allocator_);
      }
    }
  };

  // No-op function for `CurrentStruct`.
  template <typename TT = T>
  static ENABLE_IF<std::is_same<TT, CurrentStruct>::value, bool> Save(rapidjson::Value&,
                                                                      rapidjson::Document::AllocatorType&,
                                                                      const TT&,
                                                                      bool) {
    return false;
  }

  // `CURRENT_STRUCT`.
  template <typename TT = T>
  static ENABLE_IF<IS_CURRENT_STRUCT(TT) && !std::is_same<TT, CurrentStruct>::value, bool> Save(
      rapidjson::Value& destination,
      rapidjson::Document::AllocatorType& allocator,
      const TT& source,
      bool set_object_already_called = false) {
    using DECAYED_T = current::decay<TT>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (!set_object_already_called) {
      destination.SetObject();
    }
    SaveIntoJSONImpl<SUPER, J>::Save(destination, allocator, dynamic_cast<const SUPER&>(source), true);

    SaveFieldVisitor visitor(destination, allocator);
    current::reflection::VisitAllFields<DECAYED_T, current::reflection::FieldNameAndImmutableValue>::WithObject(
        source, visitor);

    return true;
  }
};

}  // namespace save

namespace load {

template <typename T, JSONFormat J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<IS_CURRENT_STRUCT(T)>> {
  struct LoadFieldVisitor {
    rapidjson::Value& source_;
    const std::string& path_;

    explicit LoadFieldVisitor(rapidjson::Value& source, const std::string& path)
        : source_(source), path_(path) {}

    // IMPORTANT: Pass in `const char* name`, as `const std::string& name`
    // would fail memory-allocation-wise due to over-smartness of RapidJSON.
    template <typename U>
    void operator()(const char* name, U& value) const {
      if (source_.HasMember(name)) {
        LoadFromJSONImpl<U, J>::Load(&source_[name], value, path_ + '.' + name);
      } else {
        LoadFromJSONImpl<U, J>::Load(nullptr, value, path_ + '.' + name);
      }
    }
  };

  // No-op function required for compilation.
  static void Load(rapidjson::Value*, CurrentStruct&, const std::string&) {}

  // `CURRENT_STRUCT`.
  template <typename TT = T>
  static ENABLE_IF<IS_CURRENT_STRUCT(TT) && !std::is_same<TT, CurrentStruct>::value> Load(
      rapidjson::Value* source, T& destination, const std::string& path) {
    using DECAYED_T = current::decay<TT>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (source && source->IsObject()) {
      if (!std::is_same<SUPER, CurrentStruct>::value) {
        LoadFromJSONImpl<SUPER, J>::Load(source, destination, path);
      }
      LoadFieldVisitor visitor(*source, path);
      current::reflection::VisitAllFields<DECAYED_T, current::reflection::FieldNameAndMutableValue>::WithObject(
          destination, visitor);
    } else {
      throw JSONSchemaException("object", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load

}  // namespace json
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_STRUCT_H


