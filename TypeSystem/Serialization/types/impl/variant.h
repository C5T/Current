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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_IMPL_VARIANT_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_IMPL_VARIANT_H

#include "../../base.h"
#include "../../../struct.h"
#include "../../../variant.h"

namespace current {
namespace serialization {
namespace json {
namespace load {

template <class J>
struct LoadVariantGenericDeserializer {
  virtual void Deserialize(rapidjson::Value* source,
                           IHasUncheckedMoveFromUniquePtr& destination,
                           const std::string& path) = 0;
};

template <typename T, typename J>
struct TypedDeserializer : LoadVariantGenericDeserializer<J> {
  explicit TypedDeserializer(const std::string& key_name) : key_name_(key_name) {}

  void Deserialize(rapidjson::Value* source,
                   IHasUncheckedMoveFromUniquePtr& destination,
                   const std::string& path) override {
    if (source->HasMember(key_name_)) {
      auto result = std::make_unique<T>();
      LoadFromJSONImpl<T, JSONFormat::Current>::Load(
          &(*source)[key_name_], *result, path + "[\"" + key_name_ + "\"]");
      destination.UncheckedMoveFromUniquePtr(std::move(result));
    } else if (!JSONPatchMode<J>::value) {
      // LCOV_EXCL_START
      throw JSONSchemaException("variant value", source, path + "[\"" + key_name_ + "\"]");
      // LCOV_EXCL_STOP
    }
  }

  const std::string key_name_;
};

template <typename T, typename J>
struct TypedDeserializerMinimalistic : LoadVariantGenericDeserializer<J> {
  void Deserialize(rapidjson::Value* source,
                   IHasUncheckedMoveFromUniquePtr& destination,
                   const std::string& path) override {
    auto result = std::make_unique<T>();
    LoadFromJSONImpl<T, JSONFormat::Minimalistic>::Load(source, *result, path);
    destination.UncheckedMoveFromUniquePtr(std::move(result));
  }
};

template <typename T, typename J>
struct TypedDeserializerFSharp : LoadVariantGenericDeserializer<J> {
  void Deserialize(rapidjson::Value* source,
                   IHasUncheckedMoveFromUniquePtr& destination,
                   const std::string& path) override {
    if (source->HasMember("Fields")) {
      rapidjson::Value* fields = &(*source)["Fields"];
      if (fields && fields->IsArray() && fields->Size() == 1u) {
        auto result = std::make_unique<T>();
        LoadFromJSONImpl<T, JSONFormat::NewtonsoftFSharp>::Load(
            &(*fields)[static_cast<rapidjson::SizeType>(0)], *result, path + ".[\"Fields\"]");
        destination.UncheckedMoveFromUniquePtr(std::move(result));
      } else {
        // No PATCH for F#. -- D.K.
        // LCOV_EXCL_START
        throw JSONSchemaException("array of one element in \"Fields\"", source, path + ".[\"Fields\"]");
        // LCOV_EXCL_STOP
      }
    } else {
      if (IS_EMPTY_CURRENT_STRUCT(T)) {
        // Allow just `"Case"` and no `"Fields"` for empty `CURRENT_STRUCT`-s.
        destination.UncheckedMoveFromUniquePtr(std::move(std::make_unique<T>()));
      } else {
        // No PATCH for F#. -- D.K.
        // LCOV_EXCL_START
        throw JSONSchemaException("data in \"Fields\"", source, path + ".[\"Fields\"]");
        // LCOV_EXCL_STOP
      }
    }
  }
};

}  // namespace load
}  // namespace json
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_IMPL_VARIANT_H
