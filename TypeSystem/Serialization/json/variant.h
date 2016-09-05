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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VARIANT_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VARIANT_H

#include <type_traits>

#include "primitives.h"
#include "typeid.h"

#include "../../variant.h"
#include "../../Reflection/reflection.h"

#include "../../../Bricks/template/combine.h"
#include "../../../Bricks/template/enable_if.h"
#include "../../../Bricks/template/mapreduce.h"

#include "impl/variant.h"

// JSON format for `Variant` objects:
// * Contains TypeID.
// * Under an empty key, so can't confuse it with the actual struct name.
// * Contains a struct name too, making it easier to use our JSON from JavaScript++.
//   The actual data is the object with the key being the name of the struct, which *would* be ambiguous
//   in case of multiple namespaces, but internally we only use TypeID during the deserialization phase.

namespace current {
namespace serialization {

namespace json {
template <json::JSONVariantStyle, class JSON_FORMAT>
class JSONVariantSerializer;

template <class JSON_FORMAT>
class JSONVariantSerializer<json::JSONVariantStyle::Current, JSON_FORMAT> {
 public:
  explicit JSONVariantSerializer(json::JSONStringifier<JSON_FORMAT>& json_stringifier)
      : json_stringifier_(json_stringifier) {}

  template <typename X>
  ENABLE_IF<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
    rapidjson::Value serialized_object;
    json_stringifier_.Inner(&serialized_object, object);

    using namespace ::current::reflection;
    rapidjson::Value serialized_type_id;
    json_stringifier_.Inner(&serialized_type_id,
                            Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);

    json_stringifier_.Current().SetObject();

    json_stringifier_.Current().AddMember(
        std::move(rapidjson::Value(CurrentTypeNameAsConstCharPtr<X>(), json_stringifier_.Allocator()).Move()),
        serialized_object,
        json_stringifier_.Allocator());

    if (json::JSONVariantTypeIDInEmptyKey<JSON_FORMAT>::value) {
      json_stringifier_.Current().AddMember(
          std::move(rapidjson::Value("", json_stringifier_.Allocator()).Move()),
          serialized_type_id,
          json_stringifier_.Allocator());
    }
    if (json::JSONVariantTypeNameInDollarKey<JSON_FORMAT>::value) {
      json_stringifier_.Current().AddMember(
          std::move(rapidjson::Value("$", json_stringifier_.Allocator()).Move()),
          std::move(rapidjson::Value(CurrentTypeNameAsConstCharPtr<X>(), json_stringifier_.Allocator()).Move()),
          json_stringifier_.Allocator());
    }
  }

 private:
  json::JSONStringifier<JSON_FORMAT>& json_stringifier_;
};

template <class JSON_FORMAT>
class JSONVariantSerializer<json::JSONVariantStyle::Simple, JSON_FORMAT>
    : public JSONVariantSerializer<json::JSONVariantStyle::Current, JSON_FORMAT> {
  using JSONVariantSerializer<json::JSONVariantStyle::Current, JSON_FORMAT>::JSONVariantSerializer;
};

template <class JSON_FORMAT>
class JSONVariantSerializer<json::JSONVariantStyle::NewtonsoftFSharp, JSON_FORMAT> {
 public:
  explicit JSONVariantSerializer(json::JSONStringifier<JSON_FORMAT>& json_stringifier)
      : json_stringifier_(json_stringifier) {}

  template <typename X>
  ENABLE_IF<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
    rapidjson::Value serialized_object;
    json_stringifier_.Inner(&serialized_object, object);

    json_stringifier_.Current().SetObject();
    json_stringifier_.Current().AddMember(
        std::move(rapidjson::Value("Case", json_stringifier_.Allocator()).Move()),
        std::move(rapidjson::Value(reflection::CurrentTypeName<X>(), json_stringifier_.Allocator()).Move()),
        json_stringifier_.Allocator());

    if (IS_CURRENT_VARIANT(X) || !IS_EMPTY_CURRENT_STRUCT(X)) {
      rapidjson::Value fields_as_array;
      fields_as_array.SetArray();
      fields_as_array.PushBack(std::move(serialized_object.Move()), json_stringifier_.Allocator());

      json_stringifier_.Current().AddMember(
          std::move(rapidjson::Value("Fields", json_stringifier_.Allocator()).Move()),
          std::move(fields_as_array.Move()),
          json_stringifier_.Allocator());
    }
  }

 private:
  json::JSONStringifier<JSON_FORMAT>& json_stringifier_;
};
}  // namespace current::serialize::json

template <class JSON_FORMAT, typename T>
struct SerializeImpl<json::JSONStringifier<JSON_FORMAT>, T, std::enable_if_t<IS_CURRENT_VARIANT(T)>> {
  static void DoSerialize(json::JSONStringifier<JSON_FORMAT>& json_stringifier, const T& value) {
    if (Exists(value)) {
      json::JSONVariantSerializer<JSON_FORMAT::variant_style, JSON_FORMAT> impl(json_stringifier);
      value.Call(impl);
    } else {
      if (json::JSONVariantStyleUseNulls<JSON_FORMAT::variant_style>::value) {
        json_stringifier.Current().SetNull();
      } else {
        json_stringifier.MarkAsAbsentValue();
      }
    }
  }
};

namespace json {
namespace load {

template <JSONVariantStyle J, class JSON_FORMAT, typename VARIANT>
struct LoadVariantImpl;

template <class JSON_FORMAT, typename VARIANT>
struct LoadVariantImpl<JSONVariantStyle::Current, JSON_FORMAT, VARIANT> {
  class Impl {
   public:
    Impl() {
      current::metaprogramming::combine<current::metaprogramming::map<Registerer, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      if (source && source->IsObject()) {
        reflection::TypeID type_id;
        if (source->HasMember("")) {
          rapidjson::Value* member = &(*source)[""];
          LoadFromJSONImpl<reflection::TypeID, JSON_FORMAT>::Load(member, type_id, path + "[\"\"]");
          const auto cit = deserializers_.find(type_id);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(source, destination, path);
          } else {
            throw JSONSchemaException("a type id listed in the type list", member, path);  // LCOV_EXCL_LINE
          }
        } else {
          throw JSONSchemaException("type id as value for an empty string", source, path);  // LCOV_EXCL_LINE
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (source && !source->IsObject())) {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t = std::unordered_map<reflection::TypeID,
                                                   std::unique_ptr<LoadVariantGenericDeserializer<JSON_FORMAT>>,
                                                   CurrentHashFunction<::current::reflection::TypeID>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct Registerer {
      void DispatchToAll(deserializers_map_t& deserializers) {
        // Silently discard duplicate types in the input type list. They would be deserialized correctly.
        deserializers[Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<X>()).type_id] =
            std::make_unique<TypedDeserializer<X, JSON_FORMAT>>(reflection::CurrentTypeName<X>());
      }
    };
  };

  static const Impl& Instance() {
    static Impl impl;
    return impl;
  }
};

template <class JSON_FORMAT, typename VARIANT>
struct LoadVariantImpl<JSONVariantStyle::Simple, JSON_FORMAT, VARIANT> {
  class ImplMinimalistic {
   public:
    ImplMinimalistic() {
      current::metaprogramming::combine<
          current::metaprogramming::map<RegistererByName, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      if (source && source->IsObject()) {
        std::string case_name = "";
        rapidjson::Value* value = nullptr;
        for (auto cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
          if (!cit->name.IsString()) {
            // Should never happen, just a sanity check. -- D.K.
            throw JSONSchemaException("key name as string", source, path);  // LCOV_EXCL_LINE
          }
          const std::string key = cit->name.GetString();
          // Skip keys "" and "$" for "backwards" compatibility with the "Current" format.
          if (!key.empty() && key != "$") {
            if (!value) {
              case_name = key;
              value = &cit->value;
            } else {
              // LCOV_EXCL_START
              throw JSONSchemaException(
                  std::string("no other key after `") + case_name + "`, seeing `" + key + "`", source, path);
              // LCOV_EXCL_STOP
            }
          }
        }
        if (!value) {
          throw JSONSchemaException("a key-value entry with a variant type", source, path);  // LCOV_EXCL_LINE
        } else {
          const auto cit = deserializers_.find(case_name);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(value, destination, path);
          } else {
            throw JSONSchemaException("the value for the variant type", value, path);  // LCOV_EXCL_LINE
          }
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (source && !source->IsObject())) {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<LoadVariantGenericDeserializer<JSON_FORMAT>>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct RegistererByName {
      void DispatchToAll(deserializers_map_t& deserializers) {
        // Silently discard duplicate types in the input type list.
        // TODO(dkorolev): This is oh so wrong here.
        deserializers[reflection::CurrentTypeName<X>()] =
            std::make_unique<TypedDeserializerMinimalistic<X, JSON_FORMAT>>();
      }
    };
  };

  static const ImplMinimalistic& Instance() {
    static ImplMinimalistic impl;
    return impl;
  }
};

template <class JSON_FORMAT, typename VARIANT>
struct LoadVariantImpl<JSONVariantStyle::NewtonsoftFSharp, JSON_FORMAT, VARIANT> {
  class ImplFSharp {
   public:
    ImplFSharp() {
      current::metaprogramming::combine<
          current::metaprogramming::map<RegistererByName, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      if (source && source->IsObject()) {
        if (source->HasMember("Case")) {
          rapidjson::Value* member = &(*source)["Case"];
          std::string case_name;
          LoadFromJSONImpl<std::string, JSON_FORMAT>::Load(member, case_name, path + ".[\"Case\"]");
          const auto cit = deserializers_.find(case_name);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(source, destination, path);
          } else {
            throw JSONSchemaException("one of requested values of \"Case\"", member, path);  // LCOV_EXCL_LINE
          }
        } else {
          throw JSONSchemaException("a type name in \"Case\"", source, path);  // LCOV_EXCL_LINE
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (source && !source->IsObject())) {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<LoadVariantGenericDeserializer<JSON_FORMAT>>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct RegistererByName {
      void DispatchToAll(deserializers_map_t& deserializers) {
        // Silently discard duplicate types in the input type list.
        // TODO(dkorolev): This is oh so wrong here.
        deserializers[reflection::CurrentTypeName<X>()] =
            std::make_unique<TypedDeserializerFSharp<X, JSON_FORMAT>>();
      }
    };
  };

  static const ImplFSharp& Instance() {
    static ImplFSharp impl;
    return impl;
  }
};

template <typename T, class J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<IS_CURRENT_VARIANT(T)>> {
  static void Load(rapidjson::Value* source, T& value, const std::string& path) {
    if (!source || source->IsNull()) {
      if (JSONVariantStyleUseNulls<J::variant_style>::value) {
        throw JSONUninitializedVariantObjectException();
      }
    } else {
      LoadVariantImpl<J::variant_style, J, T>::Instance().DoLoadVariant(source, value, path);
    }
  }
};

}  // namespace current::serialization::json::load
}  // namespace current::serialization::json
}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VARIANT_H
