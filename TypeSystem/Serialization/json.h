/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H
#define CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H

#include <type_traits>
#include <limits>

#include "exceptions.h"

#include "../struct.h"
#include "../optional.h"
#include "../variant.h"

#include "../Reflection/types.h"
#include "../Reflection/reflection.h"

#include "../../Bricks/template/enable_if.h"
#include "../../Bricks/template/decay.h"
#include "../../Bricks/strings/strings.h"
#include "../../Bricks/time/chrono.h"
#include "../../Bricks/util/singleton.h"

#define RAPIDJSON_HAS_STDSTRING 1
#include "../../3rdparty/rapidjson/document.h"
#include "../../3rdparty/rapidjson/prettywriter.h"
#include "../../3rdparty/rapidjson/streamwrapper.h"

namespace current {
namespace serialization {

// Current: as human- and JavaScript-readable as possible, with strong struct signature checking.
// Used internally, and is the default for exposed API endpoints.
// * `CURRENT_STRUCT`-s as objects with field names as keys.
// * `map<string, X>` as `{"k1":<v1>, "k2":<v2>, ...}`, `map<K, V>` as `[[<k1>, <v1>], [<k2>, <v2>], ...]`.
// * `Variant<A, B, C> x = A(...)` as `{"A":<body>, "":<type ID of A>}`.
//
// Minimalistic: same as `Current` above except for treating `Variant`s.
// Less strict. Used primarily to import data into Current from a trusted source, ex. a local Python script.
// * Does not serialize Type ID into the empty key.
// * Relies on the name of the top-level object only when doing `Variant` deserialization.
//
// NewtonsoftFSharp: the format friendly with `import Newtonsoft.JSON` from F#.
// Used to communicate with F# and/or Visual Studio.
// Difference from `Current`:
// * Variant types are stored as `{"Case":"A", "Fields":[ { <fields of A> } ]}`.
// * TODO(dkorolev): Confirm `map<K, V>` does what it should. Untested as of now; `vector<T>` works fine.

enum class JSONFormat : int { Current, Minimalistic, NewtonsoftFSharp };

using current::ThreadLocalSingleton;

template <typename T>
struct AssignToRapidJSONValueImpl {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const T& value) { destination = value; }
};

template <>
struct AssignToRapidJSONValueImpl<std::string> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::string& value) {
    destination = rapidjson::StringRef(value);
  }
};

template <>
struct AssignToRapidJSONValueImpl<std::chrono::microseconds> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::chrono::microseconds& value) {
    destination.SetInt64(value.count());
  }
};

template <>
struct AssignToRapidJSONValueImpl<std::chrono::milliseconds> {
  static void WithDedicatedTreatment(rapidjson::Value& destination, const std::chrono::milliseconds& value) {
    destination.SetInt64(value.count());
  }
};

template <typename T>
void AssignToRapidJSONValue(rapidjson::Value& destination, const T& value) {
  AssignToRapidJSONValueImpl<T>::WithDedicatedTreatment(destination, value);
}

template <typename, JSONFormat>
struct SaveIntoJSONImpl;

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, unused_current_type, unused_fsharp_type) \
  template <JSONFormat J>                                                                                      \
  struct SaveIntoJSONImpl<cpp_type, J> {                                                                       \
    static bool Save(rapidjson::Value& destination,                                                            \
                     rapidjson::Document::AllocatorType&,                                                      \
                     const cpp_type& value) {                                                                  \
      AssignToRapidJSONValue(destination, value);                                                              \
      return true;                                                                                             \
    }                                                                                                          \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

template <JSONFormat J>
struct SaveIntoJSONImpl<reflection::TypeID, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   reflection::TypeID value) {
    destination.SetString("T" + strings::ToString(static_cast<uint64_t>(value)), allocator);
    return true;
  }
};

template <typename T, JSONFormat J>
struct SaveIntoJSONImpl<std::vector<T>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::vector<T>& value) {
    destination.SetArray();
    rapidjson::Value element_to_push;
    for (const auto& element : value) {
      SaveIntoJSONImpl<T, J>::Save(element_to_push, allocator, element);
      destination.PushBack(element_to_push, allocator);
    }
    return true;
  }
};

template <typename TF, typename TS, JSONFormat J>
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

template <typename TK, typename TV, JSONFormat J, typename CMP>
struct SaveIntoJSONImpl<std::map<TK, TV, CMP>, J> {
  template <typename K = TK>
  static ENABLE_IF<std::is_same<K, std::string>::value, bool> Save(
      rapidjson::Value& destination,
      rapidjson::Document::AllocatorType& allocator,
      const std::map<TK, TV, CMP>& value) {
    destination.SetObject();
    for (const auto& element : value) {
      rapidjson::Value populated_value;
      SaveIntoJSONImpl<TV, J>::Save(populated_value, allocator, element.second);
      destination.AddMember(rapidjson::StringRef(element.first), populated_value, allocator);
    }
    return true;
  }
  template <typename K = TK>
  static ENABLE_IF<!std::is_same<K, std::string>::value, bool> Save(
      rapidjson::Value& destination,
      rapidjson::Document::AllocatorType& allocator,
      const std::map<TK, TV, CMP>& value) {
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

template <typename T, JSONFormat J>
struct SaveIntoJSONImpl<Optional<T>, J> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T>& value) {
    if (Exists(value)) {
      SaveIntoJSONImpl<T, J>::Save(destination, allocator, Value(value));
      return true;
    } else {
      // Current's default JSON parser would accept a missing field as well for no value,
      // but output it as `null` nonetheless, for clarity.
      destination.SetNull();
      return true;
    }
  }
};

template <typename T>
struct SaveIntoJSONImpl<Optional<T>, JSONFormat::Minimalistic> {
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T>& value) {
    if (Exists(value)) {
      SaveIntoJSONImpl<T, JSONFormat::Minimalistic>::Save(destination, allocator, Value(value));
      return true;
    } else {
      return false;
    }
  }
};

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T, JSONFormat J>
struct SaveIntoJSONImpl {
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

  // `enum` and `enum class`.
  template <typename TT = T>
  static ENABLE_IF<std::is_enum<TT>::value, bool> Save(rapidjson::Value& destination,
                                                       rapidjson::Document::AllocatorType&,
                                                       const TT& value) {
    AssignToRapidJSONValue(destination, static_cast<typename std::underlying_type<TT>::type>(value));
    return true;
  }

  // JSON format for `Variant` objects:
  // * Contains TypeID.
  // * Under an empty key, so can't confuse it with the actual struct name.
  // * Contains a struct name too, making it easier to use our JSON from JavaScript++.
  //   The actual data is the object with the key being the name of the struct, which *would* be ambiguous
  //   in case of multiple namespaces, but internally we only use TypeID during the deserialization phase.
  template <bool SAVE_TYPEID_AS_EMPTY_STRING_KEY>
  class SaveVariant {
   public:
    SaveVariant(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    template <typename X>
    ENABLE_IF<IS_CURRENT_STRUCT(X) || IS_VARIANT(X)> operator()(const X& object) {
      rapidjson::Value serialized_object;
      Save(serialized_object, allocator_, object);

      using namespace ::current::reflection;
      rapidjson::Value serialized_type_id;
      SaveIntoJSONImpl<TypeID, J>::Save(
          serialized_type_id, allocator_, Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);

      destination_.SetObject();
      destination_.AddMember(
          rapidjson::Value(CurrentTypeName<X>(), allocator_).Move(), serialized_object, allocator_);

      if (SAVE_TYPEID_AS_EMPTY_STRING_KEY) {
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
    ENABLE_IF<IS_CURRENT_STRUCT(X)> operator()(const X& object) {
      rapidjson::Value serialized_object;
      Save(serialized_object, allocator_, object);

      using namespace ::current::reflection;

      destination_.SetObject();
      destination_.AddMember(rapidjson::Value("Case", allocator_).Move(),
                             rapidjson::Value(CurrentTypeName<X>(), allocator_).Move(),
                             allocator_);

      rapidjson::Value fields_as_array;
      fields_as_array.SetArray();
      fields_as_array.PushBack(serialized_object.Move(), allocator_);

      destination_.AddMember(rapidjson::Value("Fields", allocator_).Move(), fields_as_array.Move(), allocator_);
    }

   private:
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;
  };
  template <typename... TS>
  static bool Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const VariantImpl<TypeListImpl<TS...>>& value) {
    // TODO(dkorolev): This call might be worth splitting into two, with and without REQUIRED. Later.
    if (Exists(value)) {
      typename std::conditional<J == JSONFormat::Current,
                                SaveVariant<true>,
                                typename std::conditional<J == JSONFormat::Minimalistic,
                                                          SaveVariant<false>,
                                                          SaveVariantFSharp>::type>::type impl(destination,
                                                                                               allocator);
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

template <JSONFormat J, typename T>
std::string CreateJSONViaRapidJSON(const T& value) {
  rapidjson::Document document;
  rapidjson::Value& destination = document;

  SaveIntoJSONImpl<T, J>::Save(destination, document.GetAllocator(), value);

  std::ostringstream os;
  rapidjson::OStreamWrapper stream(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(stream);
  document.Accept(writer);

  return os.str();
}

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T, JSONFormat J>
struct LoadFromJSONImpl {
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

  // `uint*_t`.
  template <typename TT = T>
  static ENABLE_IF<std::numeric_limits<TT>::is_integer && !std::numeric_limits<TT>::is_signed &&
                   !std::is_same<TT, bool>::value>
  Load(rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<T>(source->GetUint64());
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }

  // `int*_t`
  template <typename TT = T>
  static ENABLE_IF<std::numeric_limits<TT>::is_integer && std::numeric_limits<TT>::is_signed> Load(
      rapidjson::Value* source, T& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<T>(source->GetInt64());
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }

  // `float`.
  static void Load(rapidjson::Value* source, float& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = static_cast<float>(source->GetDouble());
    } else {
      throw JSONSchemaException("float", source, path);  // LCOV_EXCL_LINE
    }
  }

  // `double`.
  static void Load(rapidjson::Value* source, double& destination, const std::string& path) {
    if (source && source->IsNumber()) {
      destination = source->GetDouble();
    } else {
      throw JSONSchemaException("double", source, path);  // LCOV_EXCL_LINE
    }
  }

  // `TypeID`.
  static void Load(rapidjson::Value* source, reflection::TypeID& destination, const std::string& path) {
    if (source && source->IsString() && *source->GetString() == 'T') {
      destination = static_cast<reflection::TypeID>(strings::FromString<uint64_t>(source->GetString() + 1));
    } else {
      throw JSONSchemaException("TypeID", source, path);  // LCOV_EXCL_LINE
    }
  }

  // `enum` and `enum class`.
  template <typename TT = T>
  static ENABLE_IF<std::is_enum<TT>::value> Load(rapidjson::Value* source,
                                                 T& destination,
                                                 const std::string& path) {
    if (source && source->IsNumber()) {
      if (std::numeric_limits<typename std::underlying_type<T>::type>::is_signed) {
        destination = static_cast<T>(source->GetInt64());
      } else {
        destination = static_cast<T>(source->GetUint64());
      }
    } else {
      throw JSONSchemaException("number", source, path);  // LCOV_EXCL_LINE
    }
  }

  template <typename... TS>
  struct LoadVariant {
    class Impl {
     public:
      Impl() {
        current::metaprogramming::combine<current::metaprogramming::map<Registerer, TypeListImpl<TS...>>>
            bulk_deserializers_registerer;
        bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
      }

      void DoLoadVariant(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) const {
        using namespace ::current::reflection;
        if (source && source->IsObject()) {
          TypeID type_id;
          if (source->HasMember("")) {
            rapidjson::Value* member = &(*source)[""];
            LoadFromJSONImpl<TypeID, J>::Load(member, type_id, path + "[\"\"]");
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
        virtual void Deserialize(rapidjson::Value* source,
                                 VariantImpl<TypeListImpl<TS...>>& destination,
                                 const std::string& path) = 0;
      };

      template <typename X>
      struct TypedDeserializer : GenericDeserializer {
        explicit TypedDeserializer(const std::string& key_name) : key_name_(key_name) {}

        void Deserialize(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) override {
          if (source->HasMember(key_name_)) {
            destination = std::make_unique<X>();
            LoadFromJSONImpl<X, J>::Load(
                &(*source)[key_name_], Value<X>(destination), path + "[\"" + key_name_ + "\"]");
          } else {
            // LCOV_EXCL_START
            throw JSONSchemaException("variant value", source, path + "[\"" + key_name_ + "\"]");
            // LCOV_EXCL_STOP
          }
        }

        const std::string key_name_;
      };

      using T_DESERIALIZERS_MAP =
          std::unordered_map<::current::reflection::TypeID,
                             std::unique_ptr<GenericDeserializer>,
                             ::current::CurrentHashFunction<::current::reflection::TypeID>>;
      T_DESERIALIZERS_MAP deserializers_;

      template <typename X>
      struct Registerer {
        void DispatchToAll(T_DESERIALIZERS_MAP& deserializers) {
          using namespace ::current::reflection;
          // Silently discard duplicate types in the input type list. They would be deserialized correctly.
          deserializers[Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id] =
              std::make_unique<TypedDeserializer<X>>(CurrentTypeName<X>());
        }
      };
    };

    static const Impl& Instance() { return ThreadLocalSingleton<Impl>(); }
  };

  template <typename... TS>
  struct LoadVariantMinimalistic {
    class ImplMinimalistic {
     public:
      ImplMinimalistic() {
        current::metaprogramming::combine<current::metaprogramming::map<RegistererByName, TypeListImpl<TS...>>>
            bulk_deserializers_registerer;
        bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
      }

      void DoLoadVariant(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) const {
        using namespace ::current::reflection;
        if (source && source->IsObject()) {
          std::string case_name = "";
          rapidjson::Value* value = nullptr;
          for (auto cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
            if (!cit->name.IsString()) {
              // Should never happen, just a sanity check. -- D.K.
              throw JSONSchemaException("key name as string", source, path);
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
            throw JSONSchemaException("a key-value entry with a variant type", source, path);
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
        virtual void Deserialize(rapidjson::Value* source,
                                 VariantImpl<TypeListImpl<TS...>>& destination,
                                 const std::string& path) = 0;
      };

      template <typename X>
      struct TypedDeserializerMinimalistic : GenericDeserializerMinimalistic {
        void Deserialize(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) override {
          destination = std::make_unique<X>();
          LoadFromJSONImpl<X, J>::Load(source, Value<X>(destination), path);
        }
      };

      using T_DESERIALIZERS_MAP =
          std::unordered_map<std::string, std::unique_ptr<GenericDeserializerMinimalistic>>;
      T_DESERIALIZERS_MAP deserializers_;

      template <typename X>
      struct RegistererByName {
        void DispatchToAll(T_DESERIALIZERS_MAP& deserializers) {
          using namespace ::current::reflection;
          // Silently discard duplicate types in the input type list.
          // TODO(dkorolev): This is oh so wrong here.
          deserializers[CurrentTypeName<X>()] = std::make_unique<TypedDeserializerMinimalistic<X>>();
        }
      };
    };

    static const ImplMinimalistic& Instance() { return ThreadLocalSingleton<ImplMinimalistic>(); }
  };

  template <typename... TS>
  struct LoadVariantFSharp {
    class ImplFSharp {
     public:
      ImplFSharp() {
        current::metaprogramming::combine<current::metaprogramming::map<RegistererByName, TypeListImpl<TS...>>>
            bulk_deserializers_registerer;
        bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
      }

      void DoLoadVariant(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) const {
        using namespace ::current::reflection;
        if (source && source->IsObject()) {
          if (source->HasMember("Case")) {
            rapidjson::Value* member = &(*source)["Case"];
            std::string case_name;
            LoadFromJSONImpl<std::string, J>::Load(member, case_name, path + ".[\"Case\"]");
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
        virtual void Deserialize(rapidjson::Value* source,
                                 VariantImpl<TypeListImpl<TS...>>& destination,
                                 const std::string& path) = 0;
      };

      template <typename X>
      struct TypedDeserializerFSharp : GenericDeserializerFSharp {
        void Deserialize(rapidjson::Value* source,
                         VariantImpl<TypeListImpl<TS...>>& destination,
                         const std::string& path) override {
          if (source->HasMember("Fields")) {
            rapidjson::Value* fields = &(*source)["Fields"];
            if (fields && fields->IsArray() && fields->Size() == 1u) {
              destination = std::make_unique<X>();
              LoadFromJSONImpl<X, J>::Load(&(*fields)[static_cast<rapidjson::SizeType>(0)],
                                           Value<X>(destination),
                                           path + ".[\"Fields\"]");
            } else {
              throw JSONSchemaException("array of one element in \"Fields\"", source, path + ".[\"Fields\"]");
            }
          } else {
            // LCOV_EXCL_START
            throw JSONSchemaException("data in \"Fields\"", source, path + ".[\"Fields\"]");
            // LCOV_EXCL_STOP
          }
        }
      };

      using T_DESERIALIZERS_MAP = std::unordered_map<std::string, std::unique_ptr<GenericDeserializerFSharp>>;
      T_DESERIALIZERS_MAP deserializers_;

      template <typename X>
      struct RegistererByName {
        void DispatchToAll(T_DESERIALIZERS_MAP& deserializers) {
          using namespace ::current::reflection;
          // Silently discard duplicate types in the input type list.
          // TODO(dkorolev): This is oh so wrong here.
          deserializers[CurrentTypeName<X>()] = std::make_unique<TypedDeserializerFSharp<X>>();
        }
      };
    };

    static const ImplFSharp& Instance() { return ThreadLocalSingleton<ImplFSharp>(); }
  };

  template <typename... TS>
  using LoadVariantPicker =
      typename std::conditional<J == JSONFormat::Current,
                                LoadVariant<TS...>,
                                typename std::conditional<J == JSONFormat::Minimalistic,
                                                          LoadVariantMinimalistic<TS...>,
                                                          LoadVariantFSharp<TS...>>::type>::type;

  template <typename... TS>
  static void Load(rapidjson::Value* source, VariantImpl<TypeListImpl<TS...>>& value, const std::string& path) {
    if (!source || source->IsNull()) {
      if (J != JSONFormat::Minimalistic) {
        throw JSONUninitializedVariantObjectException();
      }
    } else {
      using LOADER = LoadVariantPicker<TS...>;
      LOADER::Instance().DoLoadVariant(source, value, path);
    }
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<std::string, J> {
  static void Load(rapidjson::Value* source, std::string& destination, const std::string& path) {
    if (source && source->IsString()) {
      destination.assign(source->GetString(), source->GetStringLength());
    } else {
      throw JSONSchemaException("string", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<bool, J> {
  static void Load(rapidjson::Value* source, bool& destination, const std::string& path) {
    if (source && (source->IsTrue() || source->IsFalse())) {
      destination = source->IsTrue();
    } else {
      throw JSONSchemaException("bool", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename T, JSONFormat J>
struct LoadFromJSONImpl<std::vector<T>, J> {
  static void Load(rapidjson::Value* source, std::vector<T>& destination, const std::string& path) {
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

template <typename TF, typename TS, JSONFormat J>
struct LoadFromJSONImpl<std::pair<TF, TS>, J> {
  static void Load(rapidjson::Value* source, std::pair<TF, TS>& destination, const std::string& path) {
    if (source && source->IsArray() && source->Size() == 2u) {
      LoadFromJSONImpl<TF, J>::Load(&((*source)[static_cast<rapidjson::SizeType>(0)]), destination.first, path);
      LoadFromJSONImpl<TS, J>::Load(
          &((*source)[static_cast<rapidjson::SizeType>(1)]), destination.second, path);
    } else {
      throw JSONSchemaException("pair as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename TK, typename TV, JSONFormat J, typename CMP>
struct LoadFromJSONImpl<std::map<TK, TV, CMP>, J> {
  template <typename K, typename V>
  static ENABLE_IF<std::is_same<std::string, K>::value> Load(rapidjson::Value* source,
                                                             std::map<K, V, CMP>& destination,
                                                             const std::string& path) {
    if (source && source->IsObject()) {
      destination.clear();
      K k;
      V v;
      for (rapidjson::Value::MemberIterator cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
        LoadFromJSONImpl<K, J>::Load(&cit->name, k, path);
        LoadFromJSONImpl<V, J>::Load(&cit->value, v, path);
        destination.emplace(k, v);
      }
    } else {
      throw JSONSchemaException("map as object", source, path);  // LCOV_EXCL_LINE
    }
  }
  template <typename K, typename V>
  static ENABLE_IF<!std::is_same<std::string, K>::value> Load(rapidjson::Value* source,
                                                              std::map<K, V, CMP>& destination,
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
        K k;
        V v;
        LoadFromJSONImpl<K, J>::Load(&(*cit)[static_cast<rapidjson::SizeType>(0)], k, path);
        LoadFromJSONImpl<V, J>::Load(&(*cit)[static_cast<rapidjson::SizeType>(1)], v, path);
        destination.emplace(k, v);
      }
    } else {
      throw JSONSchemaException("map as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename T, JSONFormat J>
struct LoadFromJSONImpl<Optional<T>, J> {
  static void Load(rapidjson::Value* source, Optional<T>& destination, const std::string& path) {
    if (!source || source->IsNull()) {
      destination = nullptr;
    } else {
      destination = T();
      LoadFromJSONImpl<T, J>::Load(source, Value(destination), path);
    }
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<std::chrono::milliseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::milliseconds& destination, const std::string& path) {
    uint64_t value_as_uint64;
    LoadFromJSONImpl<uint64_t, J>::Load(source, value_as_uint64, path);
    destination = std::chrono::milliseconds(value_as_uint64);
  }
};

template <JSONFormat J>
struct LoadFromJSONImpl<std::chrono::microseconds, J> {
  static void Load(rapidjson::Value* source, std::chrono::microseconds& destination, const std::string& path) {
    uint64_t value_as_uint64;
    LoadFromJSONImpl<uint64_t, J>::Load(source, value_as_uint64, path);
    destination = std::chrono::microseconds(value_as_uint64);
  }
};

template <JSONFormat J, typename T>
void ParseJSONViaRapidJSON(const std::string& json, T& destination) {
  rapidjson::Document document;

  if (document.Parse(json.c_str()).HasParseError()) {
    throw InvalidJSONException(json);
  }

  LoadFromJSONImpl<T, J>::Load(&document, destination, "");
}

template <JSONFormat J = JSONFormat::Current, typename T>
inline std::string JSON(const T& source) {
  return CreateJSONViaRapidJSON<J>(source);
}

template <JSONFormat J = JSONFormat::Current>
inline std::string JSON(const char* special_case_bare_c_string) {
  return JSON<J>(std::string(special_case_bare_c_string));
}

template <typename T, JSONFormat J = JSONFormat::Current>
inline void ParseJSON(const std::string& source, T& destination) {
  try {
    ParseJSONViaRapidJSON<J>(source, destination);
    CheckIntegrity(destination);
  } catch (UninitializedVariant) {
    throw JSONUninitializedVariantObjectException();
  }
}

template <typename T, JSONFormat J = JSONFormat::Current>
inline T ParseJSON(const std::string& source) {
  try {
    T result;
    ParseJSONViaRapidJSON<J>(source, result);
    CheckIntegrity(result);
    return result;
  } catch (UninitializedVariant) {
    throw JSONUninitializedVariantObjectException();
  }
}

}  // namespace serialization
}  // namespace current

// Inject into global namespace.
using current::serialization::JSON;
using current::serialization::ParseJSON;

using current::serialization::JSONFormat;

using current::serialization::TypeSystemParseJSONException;

using current::serialization::JSONSchemaException;
using current::serialization::InvalidJSONException;
using current::serialization::JSONUninitializedVariantObjectException;

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H
