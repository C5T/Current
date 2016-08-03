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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VARIANT_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VARIANT_H

#include <type_traits>

#include "primitive.h"

#include "current_typeid.h"

#include "../../variant.h"
#include "../../Reflection/reflection.h"

#include "../../../Bricks/template/combine.h"
#include "../../../Bricks/template/enable_if.h"
#include "../../../Bricks/template/mapreduce.h"

// JSON format for `Variant` objects:
// * Contains TypeID.
// * Under an empty key, so can't confuse it with the actual struct name.
// * Contains a struct name too, making it easier to use our JSON from JavaScript++.
//   The actual data is the object with the key being the name of the struct, which *would* be ambiguous
//   in case of multiple namespaces, but internally we only use TypeID during the deserialization phase.

namespace current {
namespace serialization {
namespace json {
namespace save {

using current::ThreadLocalSingleton;

template <typename T, JSONFormat J>
struct SaveIntoJSONImpl<T, J, ENABLE_IF<IS_CURRENT_VARIANT(T)>> {
  class SaveVariantCurrentOrMinimalistic {
   public:
    SaveVariantCurrentOrMinimalistic(rapidjson::Value& destination,
                                     rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    template <typename X>
    ENABLE_IF<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
      rapidjson::Value serialized_object;
      SaveIntoJSONImpl<X, J>::Save(serialized_object, allocator_, object);

      using namespace ::current::reflection;
      rapidjson::Value serialized_type_id;
      SaveIntoJSONImpl<TypeID, J>::Save(
          serialized_type_id, allocator_, Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);

      destination_.SetObject();

      destination_.AddMember(rapidjson::Value(CurrentTypeNameAsConstCharPtr<X>(), allocator_).Move(),
                             serialized_object,
                             allocator_);

      if (J == JSONFormat::Current) {
        destination_.AddMember(rapidjson::Value("", allocator_).Move(), serialized_type_id, allocator_);
      }
    }

   private:
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;
  };

  // Variant objects are serialized in a different way for F#.
  class SaveVariantFSharp {
   public:
    SaveVariantFSharp(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    template <typename X>
    ENABLE_IF<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
      rapidjson::Value serialized_object;
      SaveIntoJSONImpl<X, J>::Save(serialized_object, allocator_, object);

      using namespace ::current::reflection;

      destination_.SetObject();
      destination_.AddMember(rapidjson::Value("Case", allocator_).Move(),
                             rapidjson::Value(CurrentTypeName<X>(), allocator_).Move(),
                             allocator_);

      if (IS_CURRENT_VARIANT(X) || !IS_EMPTY_CURRENT_STRUCT(X)) {
        rapidjson::Value fields_as_array;
        fields_as_array.SetArray();
        fields_as_array.PushBack(serialized_object.Move(), allocator_);

        destination_.AddMember(
            rapidjson::Value("Fields", allocator_).Move(), fields_as_array.Move(), allocator_);
      }
    }

   private:
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;
  };

  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const T& value) {
    // TODO(dkorolev): This call might be worth splitting into two, with and without REQUIRED. Later.
    if (Exists(value)) {
      typename std::conditional<J == JSONFormat::Current || J == JSONFormat::Minimalistic,
                                SaveVariantCurrentOrMinimalistic,
                                SaveVariantFSharp>::type impl(destination, allocator);
      value.Call(impl);
      return true;
    } else {
      if (J != JSONFormat::Minimalistic) {
        destination.SetNull();
        return true;
      } else {
        return false;
      }
    }
  }
};
}  // namespace save

namespace load {

template <typename VARIANT>
struct LoadVariantCurrent {
  class Impl {
   public:
    Impl() {
      current::metaprogramming::combine<current::metaprogramming::map<Registerer, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      using namespace ::current::reflection;
      if (source && source->IsObject()) {
        TypeID type_id;
        if (source->HasMember("")) {
          rapidjson::Value* member = &(*source)[""];
          LoadFromJSONImpl<TypeID, JSONFormat::Current>::Load(member, type_id, path + "[\"\"]");
          const auto cit = deserializers_.find(type_id);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(source, destination, path);
          } else {
            throw JSONSchemaException("a type id listed in the type list", member, path);  // LCOV_EXCL_LINE
          }
        } else {
          throw JSONSchemaException("type id as value for an empty string", source, path);  // LCOV_EXCL_LINE
        }
      } else {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    struct GenericDeserializer {
      virtual ~GenericDeserializer() = default;
      virtual void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) = 0;
    };

    template <typename X>
    struct TypedDeserializer : GenericDeserializer {
      explicit TypedDeserializer(const std::string& key_name) : key_name_(key_name) {}

      void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) override {
        if (source->HasMember(key_name_)) {
          destination = std::make_unique<X>();
          LoadFromJSONImpl<X, JSONFormat::Current>::Load(
              &(*source)[key_name_], Value<X>(destination), path + "[\"" + key_name_ + "\"]");
        } else {
          // LCOV_EXCL_START
          throw JSONSchemaException("variant value", source, path + "[\"" + key_name_ + "\"]");
          // LCOV_EXCL_STOP
        }
      }

      const std::string key_name_;
    };

    using deserializers_map_t =
        std::unordered_map<::current::reflection::TypeID,
                           std::unique_ptr<GenericDeserializer>,
                           ::current::CurrentHashFunction<::current::reflection::TypeID>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct Registerer {
      void DispatchToAll(deserializers_map_t& deserializers) {
        using namespace ::current::reflection;
        // Silently discard duplicate types in the input type list. They would be deserialized correctly.
        deserializers[Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id] =
            std::make_unique<TypedDeserializer<X>>(CurrentTypeName<X>());
      }
    };
  };

  static const Impl& Instance() { return ThreadLocalSingleton<Impl>(); }
};

template <typename VARIANT>
struct LoadVariantMinimalistic {
  class ImplMinimalistic {
   public:
    ImplMinimalistic() {
      current::metaprogramming::combine<
          current::metaprogramming::map<RegistererByName, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      using namespace ::current::reflection;
      if (source && source->IsObject()) {
        std::string case_name = "";
        rapidjson::Value* value = nullptr;
        for (auto cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
          if (!cit->name.IsString()) {
            // Should never happen, just a sanity check. -- D.K.
            throw JSONSchemaException("key name as string", source, path);  // LCOV_EXCL_LINE
          }
          const std::string key = cit->name.GetString();
          // Skip empty key for "backwards" compatibility with the "Current" format.
          if (!key.empty()) {
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
      } else {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    struct GenericDeserializerMinimalistic {
      virtual ~GenericDeserializerMinimalistic() = default;
      virtual void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) = 0;
    };

    template <typename X>
    struct TypedDeserializerMinimalistic : GenericDeserializerMinimalistic {
      void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) override {
        destination = std::make_unique<X>();
        LoadFromJSONImpl<X, JSONFormat::Minimalistic>::Load(source, Value<X>(destination), path);
      }
    };

    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<GenericDeserializerMinimalistic>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct RegistererByName {
      void DispatchToAll(deserializers_map_t& deserializers) {
        using namespace ::current::reflection;
        // Silently discard duplicate types in the input type list.
        // TODO(dkorolev): This is oh so wrong here.
        deserializers[CurrentTypeName<X>()] = std::make_unique<TypedDeserializerMinimalistic<X>>();
      }
    };
  };

  static const ImplMinimalistic& Instance() { return ThreadLocalSingleton<ImplMinimalistic>(); }
};

template <typename VARIANT>
struct LoadVariantFSharp {
  class ImplFSharp {
   public:
    ImplFSharp() {
      current::metaprogramming::combine<
          current::metaprogramming::map<RegistererByName, typename VARIANT::typelist_t>>
          bulk_deserializers_registerer;
      bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
    }

    void DoLoadVariant(rapidjson::Value* source, VARIANT& destination, const std::string& path) const {
      using namespace ::current::reflection;
      if (source && source->IsObject()) {
        if (source->HasMember("Case")) {
          rapidjson::Value* member = &(*source)["Case"];
          std::string case_name;
          LoadFromJSONImpl<std::string, JSONFormat::NewtonsoftFSharp>::Load(
              member, case_name, path + ".[\"Case\"]");
          const auto cit = deserializers_.find(case_name);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(source, destination, path);
          } else {
            throw JSONSchemaException("one of requested values of \"Case\"", member, path);  // LCOV_EXCL_LINE
          }
        } else {
          throw JSONSchemaException("a type name in \"Case\"", source, path);  // LCOV_EXCL_LINE
        }
      } else {
        throw JSONSchemaException("variant type as object", source, path);  // LCOV_EXCL_LINE
      }
    };

   private:
    struct GenericDeserializerFSharp {
      virtual ~GenericDeserializerFSharp() = default;
      virtual void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) = 0;
    };

    template <typename X>
    struct TypedDeserializerFSharp : GenericDeserializerFSharp {
      void Deserialize(rapidjson::Value* source, VARIANT& destination, const std::string& path) override {
        if (source->HasMember("Fields")) {
          rapidjson::Value* fields = &(*source)["Fields"];
          if (fields && fields->IsArray() && fields->Size() == 1u) {
            destination = std::make_unique<X>();
            LoadFromJSONImpl<X, JSONFormat::NewtonsoftFSharp>::Load(
                &(*fields)[static_cast<rapidjson::SizeType>(0)], Value<X>(destination), path + ".[\"Fields\"]");
          } else {
            // LCOV_EXCL_START
            throw JSONSchemaException("array of one element in \"Fields\"", source, path + ".[\"Fields\"]");
            // LCOV_EXCL_STOP
          }
        } else {
          if (IS_EMPTY_CURRENT_STRUCT(X)) {
            // Allow just `"Case"` and no `"Fields"` for empty `CURRENT_STRUCT`-s.
            destination = std::make_unique<X>();
          } else {
            // LCOV_EXCL_START
            throw JSONSchemaException("data in \"Fields\"", source, path + ".[\"Fields\"]");
            // LCOV_EXCL_STOP
          }
        }
      }
    };

    using deserializers_map_t = std::unordered_map<std::string, std::unique_ptr<GenericDeserializerFSharp>>;
    deserializers_map_t deserializers_;

    template <typename X>
    struct RegistererByName {
      void DispatchToAll(deserializers_map_t& deserializers) {
        using namespace ::current::reflection;
        // Silently discard duplicate types in the input type list.
        // TODO(dkorolev): This is oh so wrong here.
        deserializers[CurrentTypeName<X>()] = std::make_unique<TypedDeserializerFSharp<X>>();
      }
    };
  };

  static const ImplFSharp& Instance() { return ThreadLocalSingleton<ImplFSharp>(); }
};

template <JSONFormat J, typename T>
using LoadVariantPicker =
    typename std::conditional<J == JSONFormat::Current,
                              LoadVariantCurrent<T>,
                              typename std::conditional<J == JSONFormat::Minimalistic,
                                                        LoadVariantMinimalistic<T>,
                                                        LoadVariantFSharp<T>>::type>::type;

template <typename T, JSONFormat J>
struct LoadFromJSONImpl<T, J, ENABLE_IF<IS_CURRENT_VARIANT(T)>> {
  static void Load(rapidjson::Value* source, T& value, const std::string& path) {
    if (!source || source->IsNull()) {
      if (J != JSONFormat::Minimalistic) {
        throw JSONUninitializedVariantObjectException();
      }
    } else {
      using LOADER = LoadVariantPicker<J, T>;
      LOADER::Instance().DoLoadVariant(source, value, path);
    }
  }
};

}  // namespace load
}  // namespace json

namespace binary {

// TODO(mzhurovich): Implement it.

}  // namespace binary
}  // namespace serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_TYPES_VARIANT_H
