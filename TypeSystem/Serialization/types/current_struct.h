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
template <class JSON_FORMAT>
class JSONStructFieldsSerializer {
 public:
  explicit JSONStructFieldsSerializer(json::JSONStringifier<JSON_FORMAT>& json_stringifier)
      : json_stringifier_(json_stringifier) {}

  // IMPORTANT: Must take name as `const char* name`, as `const std::string& name`
  // would fail memory-allocation-wise due to over-smartness of RapidJSON.
  template <typename U>
  void operator()(const char* name, const U& source) const {
    rapidjson::Value placeholder;
    if (json_stringifier_.MaybeInner(&placeholder, source)) {
      json_stringifier_.Current().AddMember(
          rapidjson::StringRef(name), std::move(placeholder.Move()), json_stringifier_.Allocator());
    }
  }

 private:
  json::JSONStringifier<JSON_FORMAT>& json_stringifier_;
};

template <class JSON_FORMAT, typename T>
struct SerializeStructImpl {
  static void SerializeStruct(JSONStructFieldsSerializer<JSON_FORMAT>& visitor, const T& source) {
    using decayed_t = current::decay<T>;
    using super_t = current::reflection::SuperType<decayed_t>;

    SerializeStructImpl<JSON_FORMAT, super_t>::SerializeStruct(visitor, source);

    current::reflection::VisitAllFields<decayed_t, current::reflection::FieldNameAndImmutableValue>::WithObject(
        source, visitor);
  }
};

template <class JSON_FORMAT>
struct SerializeStructImpl<JSON_FORMAT, CurrentStruct> {
  static void SerializeStruct(JSONStructFieldsSerializer<JSON_FORMAT>&, const CurrentStruct&) {}
};

}  // namespace current::serialization::json

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>,
                     T,
                     std::enable_if_t<IS_CURRENT_STRUCT(T) && !std::is_same<T, CurrentStruct>::value>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const T& value) {
    json_stringifier.Current().SetObject();
    json::JSONStructFieldsSerializer<JSON_FORMAT> visitor(json_stringifier);
    json::SerializeStructImpl<JSON_FORMAT, T>::SerializeStruct(visitor, value);
  }
};

namespace json {
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
    using decayed_t = current::decay<T>;
    using super_t = current::reflection::SuperType<decayed_t>;

    if (source && source->IsObject()) {
      if (!std::is_same<super_t, CurrentStruct>::value) {
        LoadFromJSONImpl<super_t, J>::Load(source, destination, path);
      }
      LoadFieldVisitor visitor(*source, path);
      current::reflection::VisitAllFields<decayed_t, current::reflection::FieldNameAndMutableValue>::WithObject(
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
  struct SerializeStructFieldVisitor {
    std::ostream& ostream_;

    explicit SerializeStructFieldVisitor(std::ostream& ostream) : ostream_(ostream) {}

    template <typename U>
    void operator()(const char*, const U& source) const {
      SaveIntoBinaryImpl<U>::Save(ostream_, source);
    }
  };

  static void Save(std::ostream& ostream, const T& source) {
    using decayed_t = current::decay<T>;
    using super_t = current::reflection::SuperType<decayed_t>;

    SaveIntoBinaryImpl<super_t>::Save(ostream, source);

    current::reflection::VisitAllFields<decayed_t, current::reflection::FieldNameAndImmutableValue>::WithObject(
        source, SerializeStructFieldVisitor(ostream));
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
    using decayed_t = current::decay<T>;
    using super_t = current::reflection::SuperType<decayed_t>;

    if (!std::is_same<super_t, CurrentStruct>::value) {
      LoadFromBinaryImpl<super_t>::Load(istream, destination);
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
