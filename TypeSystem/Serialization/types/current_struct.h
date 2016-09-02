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

#include "../base.h"

#include "../../struct.h"
#include "../../types.h"

#include "../../../Bricks/template/enable_if.h"

namespace current {
namespace serialization {
namespace json {
namespace save {

template <class J>
struct SaveIntoJSONImpl<CurrentStruct, J> {
  static bool Save(rapidjson::Value&, rapidjson::Document::AllocatorType&, const CurrentStruct&, bool) {
    return false;
  }
};

template <typename T, class J>
struct SaveIntoJSONImpl<T, J, ENABLE_IF<IS_CURRENT_STRUCT(T) && !std::is_same<T, CurrentStruct>::value>> {
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

  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const T& source,
                   bool set_object_already_called = false) {
    using DECAYED_T = current::decay<T>;
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

template <class J>
struct LoadFromJSONImpl<CurrentStruct, J> {
  static void Load(rapidjson::Value*, CurrentStruct&, const std::string&) {}
};

template <typename T, class J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<IS_CURRENT_STRUCT(T) && !std::is_same<T, CurrentStruct>::value>> {
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

  static void Load(rapidjson::Value* source, T& destination, const std::string& path) {
    using DECAYED_T = current::decay<T>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (source && source->IsObject()) {
      if (!std::is_same<SUPER, CurrentStruct>::value) {
        LoadFromJSONImpl<SUPER, J>::Load(source, destination, path);
      }
      LoadFieldVisitor visitor(*source, path);
      current::reflection::VisitAllFields<DECAYED_T, current::reflection::FieldNameAndMutableValue>::WithObject(
          destination, visitor);
    } else if (!JSONPatchMode<J>::value || (source && !source->IsObject())) {
      throw JSONSchemaException("object", source, path);  // LCOV_EXCL_LINE
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {
namespace save {

template <>
struct SaveIntoBinaryImpl<CurrentStruct> {
  static void Save(std::ostream&, const CurrentStruct&) {}
};

template <typename T>
struct SaveIntoBinaryImpl<T, ENABLE_IF<IS_CURRENT_STRUCT(T) && !std::is_same<T, CurrentStruct>::value>> {
  struct SaveFieldVisitor {
    std::ostream& ostream_;

    explicit SaveFieldVisitor(std::ostream& ostream) : ostream_(ostream) {}

    template <typename U>
    void operator()(const char*, const U& source) const {
      SaveIntoBinaryImpl<U>::Save(ostream_, source);
    }
  };

  static void Save(std::ostream& ostream, const T& source) {
    using DECAYED_T = current::decay<T>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    SaveIntoBinaryImpl<SUPER>::Save(ostream, source);

    SaveFieldVisitor visitor(ostream);
    current::reflection::VisitAllFields<DECAYED_T, current::reflection::FieldNameAndImmutableValue>::WithObject(
        source, visitor);
  }
};

}  // namespace save

namespace load {

template <>
struct LoadFromBinaryImpl<CurrentStruct> {
  static void Load(std::istream&, CurrentStruct&) {}
};

template <typename T>
struct LoadFromBinaryImpl<T, ENABLE_IF<IS_CURRENT_STRUCT(T) && !std::is_same<T, CurrentStruct>::value>> {
  struct LoadFieldVisitor {
    std::istream& istream_;

    explicit LoadFieldVisitor(std::istream& istream) : istream_(istream) {}

    template <typename U>
    void operator()(const char*, U& value) const {
      LoadFromBinaryImpl<U>::Load(istream_, value);
    }
  };

  static void Load(std::istream& istream, T& destination) {
    using DECAYED_T = current::decay<T>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (!std::is_same<SUPER, CurrentStruct>::value) {
      LoadFromBinaryImpl<SUPER>::Load(istream, destination);
    }

    LoadFieldVisitor visitor(istream);
    current::reflection::VisitAllFields<current::decay<T>,
                                        current::reflection::FieldNameAndMutableValue>::WithObject(destination,
                                                                                                   visitor);
  }
};

}  // namespace load
}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_CURRENT_STRUCT_H
