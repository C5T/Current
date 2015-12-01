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
#include "../polymorphic.h"

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

template <typename T>
struct SaveIntoJSONImpl;

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, unused_current_type, unused_fsharp_type) \
  template <>                                                                                                  \
  struct SaveIntoJSONImpl<cpp_type> {                                                                          \
    static void Save(rapidjson::Value& destination,                                                            \
                     rapidjson::Document::AllocatorType&,                                                      \
                     const cpp_type& value) {                                                                  \
      AssignToRapidJSONValue(destination, value);                                                              \
    }                                                                                                          \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

template <>
struct SaveIntoJSONImpl<reflection::TypeID> {
  static void Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   reflection::TypeID value) {
    destination.SetString("T" + strings::ToString(static_cast<uint64_t>(value)), allocator);
  }
};

template <typename T>
struct SaveIntoJSONImpl<std::vector<T>> {
  static void Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::vector<T>& value) {
    destination.SetArray();
    rapidjson::Value element_to_push;
    for (const auto& element : value) {
      SaveIntoJSONImpl<T>::Save(element_to_push, allocator, element);
      destination.PushBack(element_to_push, allocator);
    }
  }
};

template <typename TF, typename TS>
struct SaveIntoJSONImpl<std::pair<TF, TS>> {
  static void Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const std::pair<TF, TS>& value) {
    destination.SetArray();
    rapidjson::Value first_value;
    rapidjson::Value second_value;
    SaveIntoJSONImpl<TF>::Save(first_value, allocator, value.first);
    SaveIntoJSONImpl<TS>::Save(second_value, allocator, value.second);
    destination.PushBack(first_value, allocator);
    destination.PushBack(second_value, allocator);
  }
};

template <typename TK, typename TV, typename CMP>
struct SaveIntoJSONImpl<std::map<TK, TV, CMP>> {
  template <typename K = TK>
  static ENABLE_IF<std::is_same<K, std::string>::value> Save(rapidjson::Value& destination,
                                                             rapidjson::Document::AllocatorType& allocator,
                                                             const std::map<TK, TV, CMP>& value) {
    destination.SetObject();
    for (const auto& element : value) {
      rapidjson::Value populated_value;
      SaveIntoJSONImpl<TV>::Save(populated_value, allocator, element.second);
      destination.AddMember(rapidjson::StringRef(element.first), populated_value, allocator);
    }
  }
  template <typename K = TK>
  static ENABLE_IF<!std::is_same<K, std::string>::value> Save(rapidjson::Value& destination,
                                                              rapidjson::Document::AllocatorType& allocator,
                                                              const std::map<TK, TV, CMP>& value) {
    destination.SetArray();
    for (const auto& element : value) {
      rapidjson::Value key_value_as_array;
      key_value_as_array.SetArray();
      rapidjson::Value populated_key;
      rapidjson::Value populated_value;
      SaveIntoJSONImpl<TK>::Save(populated_key, allocator, element.first);
      SaveIntoJSONImpl<TV>::Save(populated_value, allocator, element.second);
      key_value_as_array.PushBack(populated_key, allocator);
      key_value_as_array.PushBack(populated_value, allocator);
      destination.PushBack(key_value_as_array, allocator);
    }
  }
};

template <typename T, bool STRIPPED>
struct SaveIntoJSONImpl<Optional<T, STRIPPED>> {
  static void Save(rapidjson::Value& destination,
                   rapidjson::Document::AllocatorType& allocator,
                   const Optional<T, STRIPPED>& value) {
    if (Exists(value)) {
      SaveIntoJSONImpl<T>::Save(destination, allocator, Value(value));
    } else {
      // Current's default JSON parser would accept a missing field as well for no value,
      // but output it as `null` nonetheless, for clarity.
      destination.SetNull();
    }
  }
};

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T>
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
      SaveIntoJSONImpl<U>::Save(placeholder, allocator_, source);
      destination_.AddMember(rapidjson::StringRef(name), placeholder, allocator_);
    }
  };

  // No-op function for `CurrentSuper`.
  template <typename TT = T>
  static ENABLE_IF<std::is_same<TT, CurrentSuper>::value> Save(rapidjson::Value&,
                                                               rapidjson::Document::AllocatorType&,
                                                               const TT&,
                                                               bool) {}
  // `CURRENT_STRUCT`.
  template <typename TT = T>
  static ENABLE_IF<IS_CURRENT_STRUCT(TT) && !std::is_same<TT, CurrentSuper>::value> Save(
      rapidjson::Value& destination,
      rapidjson::Document::AllocatorType& allocator,
      const TT& source,
      bool set_object_already_called = false) {
    using DECAYED_T = current::decay<TT>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (!set_object_already_called) {
      destination.SetObject();
    }
    SaveIntoJSONImpl<SUPER>::Save(destination, allocator, dynamic_cast<const SUPER&>(source), true);

    SaveFieldVisitor visitor(destination, allocator);
    current::reflection::VisitAllFields<DECAYED_T, current::reflection::FieldNameAndImmutableValue>::WithObject(
        source, visitor);
  }

  // `enum` and `enum class`.
  template <typename TT = T>
  static ENABLE_IF<std::is_enum<TT>::value> Save(rapidjson::Value& destination,
                                                 rapidjson::Document::AllocatorType&,
                                                 const TT& value) {
    AssignToRapidJSONValue(destination, static_cast<typename std::underlying_type<TT>::type>(value));
  }

  // JSON format for polymorphic objects:
  // * Contains TypeID.
  // * Under an empty key, so can't confuse it with the actual struct name.
  // * Contains a struct name too, making it easier to use our JSON from JavaScript++.
  //   The actual data is the object with the key being the name of the struct, which *would* be ambiguous
  //   in case of multiple namespaces, but internally we only use TypeID during the deserialization phase.
  class SavePolymorphic {
   public:
    SavePolymorphic(rapidjson::Value& destination, rapidjson::Document::AllocatorType& allocator)
        : destination_(destination), allocator_(allocator) {}

    template <typename X>
    ENABLE_IF<IS_CURRENT_STRUCT(X)> operator()(const X& object) {
      rapidjson::Value serialized_object;
      Save(serialized_object, allocator_, object);

      using namespace ::current::reflection;
      rapidjson::Value serialized_type_id;
      SaveIntoJSONImpl<TypeID>::Save(
          serialized_type_id, allocator_, Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);

      destination_.SetObject();
      destination_.AddMember(
          rapidjson::Value(CurrentTypeName<X>(), allocator_).Move(), serialized_object, allocator_);
      destination_.AddMember(rapidjson::Value("", allocator_).Move(), serialized_type_id, allocator_);
    }

   private:
    rapidjson::Value& destination_;
    rapidjson::Document::AllocatorType& allocator_;
  };

  template <bool STRIPPED, bool REQUIRED, typename STRIPPED_TYPE_LIST, typename... TS>
  static void Save(
      rapidjson::Value& destination,
      rapidjson::Document::AllocatorType& allocator,
      const GenericPolymorphicImpl<STRIPPED, REQUIRED, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& value) {
    if (Exists(value)) {
      SavePolymorphic impl(destination, allocator);
      value.Call(impl);
    } else {
      destination.SetNull();
    }
  }
};

template <typename T>
std::string CreateJSONViaRapidJSON(const T& value) {
  rapidjson::Document document;
  rapidjson::Value& destination = document;

  SaveIntoJSONImpl<T>::Save(destination, document.GetAllocator(), value);

  std::ostringstream os;
  rapidjson::OStreamWrapper stream(os);
  rapidjson::Writer<rapidjson::OStreamWrapper> writer(stream);
  document.Accept(writer);

  return os.str();
}

// TODO(dkorolev): A smart `enable_if` to not treat any non-primitive type as a `CURRENT_STRUCT`?
template <typename T>
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
        LoadFromJSONImpl<U>::Load(&source_[name], value, path_ + '.' + name);
      } else {
        LoadFromJSONImpl<U>::Load(nullptr, value, path_ + '.' + name);
      }
    }
  };

  // No-op function required for compilation.
  static void Load(rapidjson::Value*, CurrentSuper&, const std::string&) {}

  // `CURRENT_STRUCT`.
  template <typename TT = T>
  static ENABLE_IF<IS_CURRENT_STRUCT(TT) && !std::is_same<TT, CurrentSuper>::value> Load(
      rapidjson::Value* source, T& destination, const std::string& path) {
    using DECAYED_T = current::decay<TT>;
    using SUPER = current::reflection::SuperType<DECAYED_T>;

    if (source && source->IsObject()) {
      if (!std::is_same<SUPER, CurrentSuper>::value) {
        LoadFromJSONImpl<SUPER>::Load(source, destination, path);
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

  template <bool STRIPPED, bool REQUIRED, typename STRIPPED_TYPE_LIST, typename... TS>
  struct LoadPolymorphic {
    class Impl {
     public:
      Impl() {
        current::metaprogramming::combine<current::metaprogramming::map<Registerer, TypeListImpl<TS...>>>
            bulk_deserializers_registerer;
        bulk_deserializers_registerer.DispatchToAll(std::ref(deserializers_));
      }

      void DoLoadPolymorphic(
          rapidjson::Value* source,
          GenericPolymorphicImpl<STRIPPED, REQUIRED, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& destination,
          const std::string& path) const {
        using namespace ::current::reflection;
        if (source && source->IsObject()) {
          TypeID type_id;
          if (source->HasMember("")) {
            auto member = &(*source)[""];
            LoadFromJSONImpl<TypeID>::Load(member, type_id, path + "[\"\"]");
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
          throw JSONSchemaException("polymorphic type as object", source, path);  // LCOV_EXCL_LINE
        }
      };

     private:
      struct GenericDeserializer {
        virtual void Deserialize(
            rapidjson::Value* source,
            GenericPolymorphicImpl<STRIPPED, REQUIRED, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& destination,
            const std::string& path) = 0;
      };

      template <typename X>
      struct TypedDeserializer : GenericDeserializer {
        explicit TypedDeserializer(const std::string& key_name) : key_name_(key_name) {}

        void Deserialize(
            rapidjson::Value* source,
            GenericPolymorphicImpl<STRIPPED, REQUIRED, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& destination,
            const std::string& path) override {
          if (source->HasMember(key_name_)) {
            destination = MakeUnique<X>();
            LoadFromJSONImpl<X>::Load(
                &(*source)[key_name_], Value<X>(destination), path + "[\"" + key_name_ + "\"]");
          } else {
            // LCOV_EXCL_START
            throw JSONSchemaException("polymorphic value", source, path + "[\"" + key_name_ + "\"]");
            // LCOV_EXCL_STOP
          }
        }

        const std::string key_name_;
      };

      using T_DESERIALIZERS_MAP =
          std::unordered_map<::current::reflection::TypeID, std::unique_ptr<GenericDeserializer>>;
      T_DESERIALIZERS_MAP deserializers_;

      template <typename X>
      struct Registerer {
        void DispatchToAll(T_DESERIALIZERS_MAP& deserializers) {
          using namespace ::current::reflection;
          // Silently discard duplicate types in the input type list. They would be deserialized correctly.
          deserializers[Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id] =
              make_unique<TypedDeserializer<X>>(CurrentTypeName<X>());
        }
      };
    };

    static const Impl& Instance() { return ThreadLocalSingleton<Impl>(); }
  };
  template <bool STRIPPED, typename STRIPPED_TYPE_LIST, typename... TS>
  static void Load(rapidjson::Value* source,
                   GenericPolymorphicImpl<STRIPPED, false, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& value,
                   const std::string& path) {
    if (!source || source->IsNull()) {
      value = nullptr;
    } else {
      LoadPolymorphic<STRIPPED, false, STRIPPED_TYPE_LIST, TS...>::Instance().DoLoadPolymorphic(
          source, value, path);
    }
  }
  template <bool STRIPPED, typename STRIPPED_TYPE_LIST, typename... TS>
  static void Load(rapidjson::Value* source,
                   GenericPolymorphicImpl<STRIPPED, true, STRIPPED_TYPE_LIST, TypeListImpl<TS...>>& value,
                   const std::string& path) {
    if (!source || source->IsNull()) {
      throw JSONUninitializedPolymorphicObjectException();
    } else {
      LoadPolymorphic<STRIPPED, true, STRIPPED_TYPE_LIST, TS...>::Instance().DoLoadPolymorphic(
          source, value, path);
    }
  }
};

template <>
struct LoadFromJSONImpl<std::string> {
  static void Load(rapidjson::Value* source, std::string& destination, const std::string& path) {
    if (source && source->IsString()) {
      destination.assign(source->GetString(), source->GetStringLength());
    } else {
      throw JSONSchemaException("string", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <>
struct LoadFromJSONImpl<bool> {
  static void Load(rapidjson::Value* source, bool& destination, const std::string& path) {
    if (source && (source->IsTrue() || source->IsFalse())) {
      destination = source->IsTrue();
    } else {
      throw JSONSchemaException("bool", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename T>
struct LoadFromJSONImpl<std::vector<T>> {
  static void Load(rapidjson::Value* source, std::vector<T>& destination, const std::string& path) {
    if (source && source->IsArray()) {
      const size_t size = source->Size();
      destination.resize(size);
      for (rapidjson::SizeType i = 0; i < static_cast<rapidjson::SizeType>(size); ++i) {
        LoadFromJSONImpl<T>::Load(&((*source)[i]), destination[i], path + '[' + std::to_string(i) + ']');
      }
    } else {
      throw JSONSchemaException("array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename TF, typename TS>
struct LoadFromJSONImpl<std::pair<TF, TS>> {
  static void Load(rapidjson::Value* source, std::pair<TF, TS>& destination, const std::string& path) {
    if (source && source->IsArray() && source->Size() == 2u) {
      LoadFromJSONImpl<TF>::Load(&((*source)[static_cast<rapidjson::SizeType>(0)]), destination.first, path);
      LoadFromJSONImpl<TS>::Load(&((*source)[static_cast<rapidjson::SizeType>(1)]), destination.second, path);
    } else {
      throw JSONSchemaException("pair as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename TK, typename TV, typename CMP>
struct LoadFromJSONImpl<std::map<TK, TV, CMP>> {
  template <typename K, typename V>
  static ENABLE_IF<std::is_same<std::string, K>::value> Load(rapidjson::Value* source,
                                                             std::map<K, V, CMP>& destination,
                                                             const std::string& path) {
    if (source && source->IsObject()) {
      destination.clear();
      Stripped<K> k;
      Stripped<V> v;
      for (rapidjson::Value::MemberIterator cit = source->MemberBegin(); cit != source->MemberEnd(); ++cit) {
        LoadFromJSONImpl<Stripped<K>>::Load(&cit->name, k, path);
        LoadFromJSONImpl<Stripped<V>>::Load(&cit->value, v, path);
        destination.emplace(MoveFromStripped<K>(std::move(k)), MoveFromStripped<V>(std::move(v)));
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
        Stripped<K> k;
        Stripped<V> v;
        LoadFromJSONImpl<Stripped<K>>::Load(&(*cit)[static_cast<rapidjson::SizeType>(0)], k, path);
        LoadFromJSONImpl<Stripped<V>>::Load(&(*cit)[static_cast<rapidjson::SizeType>(1)], v, path);
        destination.emplace(MoveFromStripped<K>(std::move(k)), MoveFromStripped<V>(std::move(v)));
      }
    } else {
      throw JSONSchemaException("map as array", source, path);  // LCOV_EXCL_LINE
    }
  }
};

template <typename T, bool STRIPPED>
struct LoadFromJSONImpl<Optional<T, STRIPPED>> {
  static void Load(rapidjson::Value* source, Optional<T, STRIPPED>& destination, const std::string& path) {
    if (!source || source->IsNull()) {
      destination = nullptr;
    } else {
      destination = T();
      LoadFromJSONImpl<T>::Load(source, Value(destination), path);
    }
  }
};

template <>
struct LoadFromJSONImpl<std::chrono::microseconds> {
  static void Load(rapidjson::Value* source, std::chrono::microseconds& destination, const std::string& path) {
    uint64_t value_as_uint64;
    LoadFromJSONImpl<uint64_t>::Load(source, value_as_uint64, path);
    destination = std::chrono::microseconds(value_as_uint64);
  }
};

template <typename T>
void ParseJSONViaRapidJSON(const std::string& json, T& destination) {
  rapidjson::Document document;

  if (document.Parse(json.c_str()).HasParseError()) {
    throw InvalidJSONException(json);
  }

  LoadFromJSONImpl<T>::Load(&document, destination, "");
}

template <typename T>
inline std::string JSON(const T& source) {
  return CreateJSONViaRapidJSON(source);
}

inline std::string JSON(const char* special_case_bare_c_string) {
  return JSON(std::string(special_case_bare_c_string));
}

template <typename T>
inline void ParseJSON(const std::string& source, T& destination) {
  try {
    ParseJSONViaRapidJSON(source, *reinterpret_cast<Stripped<T>*>(&destination));
    CheckIntegrity(destination);
  } catch (UninitializedRequiredPolymorphic) {
    throw JSONUninitializedPolymorphicObjectException();
  }
}

template <typename T>
inline T ParseJSON(const std::string& source) {
  try {
    Stripped<T> result;
    ParseJSONViaRapidJSON(source, result);
    T& result_to_return = *reinterpret_cast<T*>(&result);
    CheckIntegrity(result_to_return);
    return MoveFromStripped<T>(std::move(result));
  } catch (UninitializedRequiredPolymorphic) {
    throw JSONUninitializedPolymorphicObjectException();
  }
}

}  // namespace serialization
}  // namespace current

// Inject into global namespace.
using current::serialization::JSON;
using current::serialization::ParseJSON;

using current::serialization::TypeSystemParseJSONException;
using current::serialization::JSONSchemaException;
using current::serialization::InvalidJSONException;
using current::serialization::JSONUninitializedPolymorphicObjectException;

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_H
