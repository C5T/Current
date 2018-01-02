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
#include "../../reflection/reflection.h"

#include "../../../bricks/template/call_all_constructors.h"
#include "../../../bricks/template/enable_if.h"
#include "../../../bricks/template/mapreduce.h"

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
  std::enable_if_t<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
    rapidjson::Value serialized_object;
    json_stringifier_.Inner(&serialized_object, object);

    using namespace ::current::reflection;
    rapidjson::Value serialized_type_id;
    json_stringifier_.Inner(&serialized_type_id, Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);

    json_stringifier_.Current().SetObject();

    json_stringifier_.Current().AddMember(rapidjson::StringRef(CurrentTypeNameAsConstCharPtr<X>()),
                                          std::move(serialized_object.Move()),
                                          json_stringifier_.Allocator());

    if (json::JSONVariantTypeIDInEmptyKey<JSON_FORMAT>::value) {
      json_stringifier_.Current().AddMember("", std::move(serialized_type_id.Move()), json_stringifier_.Allocator());
    }
    if (json::JSONVariantTypeNameInDollarKey<JSON_FORMAT>::value) {
      json_stringifier_.Current().AddMember(
          "$", rapidjson::StringRef(CurrentTypeNameAsConstCharPtr<X>()), json_stringifier_.Allocator());
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
  std::enable_if_t<IS_CURRENT_STRUCT_OR_VARIANT(X)> operator()(const X& object) {
    rapidjson::Value serialized_object;
    json_stringifier_.Inner(&serialized_object, object);

    json_stringifier_.Current().SetObject();
    json_stringifier_.Current().AddMember(
        "Case", rapidjson::StringRef(reflection::CurrentTypeNameAsConstCharPtr<X>()), json_stringifier_.Allocator());

    if (IS_CURRENT_VARIANT(X) || !IS_EMPTY_CURRENT_STRUCT(X)) {
      rapidjson::Value fields_as_array;
      fields_as_array.SetArray();
      fields_as_array.PushBack(std::move(serialized_object.Move()), json_stringifier_.Allocator());

      json_stringifier_.Current().AddMember(std::move(rapidjson::Value("Fields", json_stringifier_.Allocator()).Move()),
                                            std::move(fields_as_array.Move()),
                                            json_stringifier_.Allocator());
    }
  }

 private:
  json::JSONStringifier<JSON_FORMAT>& json_stringifier_;
};

template <class JSON_FORMAT>
class JSONVariantCaseAbstractBase {
 public:
  virtual ~JSONVariantCaseAbstractBase() = default;
  virtual void Deserialize(JSONParser<JSON_FORMAT>& json_parser, IHasUncheckedMoveFromUniquePtr& destination) = 0;
};

template <class JSON_FORMAT, typename T>
class JSONVariantCaseGeneric : public JSONVariantCaseAbstractBase<JSON_FORMAT> {
 public:
  explicit JSONVariantCaseGeneric(const char* key_name) : key_name_(key_name) {}

  void Deserialize(JSONParser<JSON_FORMAT>& json_parser, IHasUncheckedMoveFromUniquePtr& destination) override {
    if (json_parser && json_parser.Current().HasMember(key_name_)) {
      auto result = std::make_unique<T>();
      json_parser.Inner(&json_parser.Current()[key_name_], *result, "[\"", key_name_, "\"]");
      destination.UncheckedMoveFromUniquePtr(std::move(result));
    } else if (!JSONPatchMode<JSON_FORMAT>::value) {
      // LCOV_EXCL_START
      CURRENT_THROW(JSONSchemaException("variant case `" + std::string(key_name_) + "`", json_parser));
      // LCOV_EXCL_STOP
    }
  }

 private:
  const char* key_name_;
};

template <typename T, typename JSON_FORMAT>
class JSONVariantCaseMinimalistic : public JSONVariantCaseAbstractBase<JSON_FORMAT> {
 public:
  explicit JSONVariantCaseMinimalistic(const char* key_name) : key_name_(key_name) {}

  void Deserialize(JSONParser<JSON_FORMAT>& json_parser, IHasUncheckedMoveFromUniquePtr& destination) override {
    auto result = std::make_unique<T>();
    json_parser.Inner(&json_parser.Current()[key_name_], *result, "[\"", key_name_, "\"]");
    destination.UncheckedMoveFromUniquePtr(std::move(result));
  }

 private:
  const char* key_name_;
};

template <typename T, typename JSON_FORMAT>
class JSONVariantCaseFSharp : public JSONVariantCaseAbstractBase<JSON_FORMAT> {
 public:
  void Deserialize(JSONParser<JSON_FORMAT>& json_parser, IHasUncheckedMoveFromUniquePtr& destination) override {
    if (json_parser.Current().HasMember("Fields")) {
      rapidjson::Value& fields = json_parser.Current()["Fields"];
      if (fields.IsArray() && fields.Size() == 1u) {
        auto result = std::make_unique<T>();
        json_parser.Inner(&fields[static_cast<rapidjson::SizeType>(0)], *result, ".", "Fields[0]");
        destination.UncheckedMoveFromUniquePtr(std::move(result));
      } else {
        // No PATCH for F#. -- D.K.
        // LCOV_EXCL_START
        CURRENT_THROW(JSONSchemaException("array of one element in `Fields`", json_parser));
        // LCOV_EXCL_STOP
      }
    } else {
      if (IS_EMPTY_CURRENT_STRUCT(T)) {
        // Allow just `"Case"` and no `"Fields"` for empty `CURRENT_STRUCT`-s.
        destination.UncheckedMoveFromUniquePtr(std::move(std::make_unique<T>()));
      } else {
        // No PATCH for F#. -- D.K.
        // LCOV_EXCL_START
        CURRENT_THROW(JSONSchemaException("data in `Fields`", json_parser));
        // LCOV_EXCL_STOP
      }
    }
  }
};

template <JSONVariantStyle J, class JSON_FORMAT, typename VARIANT>
class JSONVariantPerStyle;

template <class JSON_FORMAT>
struct JSONVariantPerStyleRegisterer {
  template <typename X>
  struct StyleCurrent {
    using deserializers_map_t = std::unordered_map<reflection::TypeID,
                                                   std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>,
                                                   GenericHashFunction<::current::reflection::TypeID>>;

    StyleCurrent(deserializers_map_t& deserializers) {
      // Silently discard duplicate types in the input type list. They would be deserialized correctly.
      deserializers[Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<X>()).type_id] =
          std::make_unique<JSONVariantCaseGeneric<JSON_FORMAT, X>>(reflection::CurrentTypeNameAsConstCharPtr<X>());
    }
  };

  template <typename X>
  struct StyleSimple {
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>>;
    StyleSimple(deserializers_map_t& deserializers) {
      // Silently discard duplicate types in the input type list.
      // TODO(dkorolev): This is oh so wrong here.
      const char* name = reflection::CurrentTypeNameAsConstCharPtr<X>();
      deserializers[name] = std::make_unique<JSONVariantCaseMinimalistic<X, JSON_FORMAT>>(name);
    }
  };

  template <typename X>
  struct StyleFSharp {
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>>;
    StyleFSharp(deserializers_map_t& deserializers) {
      // Silently discard duplicate types in the input type list.
      // TODO(dkorolev): This is oh so wrong here.
      deserializers[reflection::CurrentTypeNameAsConstCharPtr<X>()] =
          std::make_unique<JSONVariantCaseFSharp<X, JSON_FORMAT>>();
    }
  };
};

template <class JSON_FORMAT, typename VARIANT>
class JSONVariantPerStyle<JSONVariantStyle::Current, JSON_FORMAT, VARIANT> {
 public:
  template <typename X>
  using Registerer = typename JSONVariantPerStyleRegisterer<JSON_FORMAT>::template StyleCurrent<X>;

  class Impl {
   public:
    Impl() {
      current::metaprogramming::call_all_constructors_with<Registerer,
                                                           deserializers_map_t,
                                                           typename VARIANT::typelist_t>(deserializers_);
    }

    void DoLoadVariant(JSONParser<JSON_FORMAT>& json_parser, VARIANT& destination) const {
      if (json_parser && json_parser.Current().IsObject()) {
        reflection::TypeID type_id;
        if (json_parser.Current().HasMember("")) {
          json_parser.Inner(&json_parser.Current()[""], type_id, "[\"\"]");
          const auto cit = deserializers_.find(type_id);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(json_parser, destination);
          } else {
            CURRENT_THROW(JSONSchemaException("a type id listed in the type list", json_parser));
          }
        } else {
          CURRENT_THROW(JSONSchemaException("type id as value for an empty string", json_parser));  // LCOV_EXCL_LINE
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsObject())) {
        CURRENT_THROW(JSONSchemaException("variant type as object", json_parser));  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t = std::unordered_map<reflection::TypeID,
                                                   std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>,
                                                   GenericHashFunction<::current::reflection::TypeID>>;
    deserializers_map_t deserializers_;
  };

  static const Impl& Instance() {
    static Impl impl;
    return impl;
  }
};

template <class JSON_FORMAT, typename VARIANT>
class JSONVariantPerStyle<JSONVariantStyle::Simple, JSON_FORMAT, VARIANT> {
 public:
  template <typename X>
  using RegistererByName = typename JSONVariantPerStyleRegisterer<JSON_FORMAT>::template StyleSimple<X>;

  class ImplMinimalistic {
   public:
    ImplMinimalistic() {
      current::metaprogramming::call_all_constructors_with<RegistererByName,
                                                           deserializers_map_t,
                                                           typename VARIANT::typelist_t>(deserializers_);
    }

    void DoLoadVariant(JSONParser<JSON_FORMAT>& json_parser, VARIANT& destination) const {
      if (json_parser && json_parser.Current().IsObject()) {
        std::string case_name = "";
        rapidjson::Value* value = nullptr;
        for (auto cit = json_parser.Current().MemberBegin(); cit != json_parser.Current().MemberEnd(); ++cit) {
          if (!cit->name.IsString()) {
            // Should never happen, just a sanity check. -- D.K.
            CURRENT_THROW(JSONSchemaException("key name as string", json_parser));  // LCOV_EXCL_LINE
          }
          const std::string key = cit->name.GetString();
          // Skip keys "" and "$" for "backwards" compatibility with the "Current" format.
          if (!key.empty() && key != "$") {
            if (!value) {
              case_name = key;
              value = &cit->value;
            } else {
              // LCOV_EXCL_START
              CURRENT_THROW(JSONSchemaException(
                  std::string("no other key after `") + case_name + "`, seeing `" + key + "`", json_parser));
              // LCOV_EXCL_STOP
            }
          }
        }
        if (!value) {
          CURRENT_THROW(JSONSchemaException("a key-value entry with a variant type", json_parser));  // LCOV_EXCL_LINE
        } else {
          const auto cit = deserializers_.find(case_name);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(json_parser, destination);
          } else {
            CURRENT_THROW(JSONSchemaException("variant case `" + case_name + "`", json_parser));
          }
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsObject())) {
        CURRENT_THROW(JSONSchemaException("variant type as object", json_parser));  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>>;
    deserializers_map_t deserializers_;
  };

  static const ImplMinimalistic& Instance() {
    static ImplMinimalistic impl;
    return impl;
  }
};

template <class JSON_FORMAT, typename VARIANT>
class JSONVariantPerStyle<JSONVariantStyle::NewtonsoftFSharp, JSON_FORMAT, VARIANT> {
 public:
  template <typename X>
  using RegistererByName = typename JSONVariantPerStyleRegisterer<JSON_FORMAT>::template StyleFSharp<X>;

  class ImplFSharp {
   public:
    ImplFSharp() {
      current::metaprogramming::call_all_constructors_with<RegistererByName,
                                                           deserializers_map_t,
                                                           typename VARIANT::typelist_t>(deserializers_);
    }

    void DoLoadVariant(JSONParser<JSON_FORMAT>& json_parser, VARIANT& destination) const {
      if (json_parser && json_parser.Current().IsObject()) {
        if (json_parser.Current().HasMember("Case")) {
          std::string case_name;
          json_parser.Inner(&json_parser.Current()["Case"], case_name, ".", "Case");
          const auto cit = deserializers_.find(case_name);
          if (cit != deserializers_.end()) {
            cit->second->Deserialize(json_parser, destination);
          } else {
            CURRENT_THROW(JSONSchemaException("one of requested values of \"Case\"", json_parser));  // LCOV_EXCL_LINE
          }
        } else {
          CURRENT_THROW(JSONSchemaException("a type name in \"Case\"", json_parser));  // LCOV_EXCL_LINE
        }
      } else if (!JSONPatchMode<JSON_FORMAT>::value || (json_parser && !json_parser.Current().IsObject())) {
        CURRENT_THROW(JSONSchemaException("variant type as object", json_parser));  // LCOV_EXCL_LINE
      }
    };

   private:
    using deserializers_map_t =
        std::unordered_map<std::string, std::unique_ptr<JSONVariantCaseAbstractBase<JSON_FORMAT>>>;
    deserializers_map_t deserializers_;
  };

  static const ImplFSharp& Instance() {
    static ImplFSharp impl;
    return impl;
  }
};

}  // namespace current::serialization::json

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

template <class JSON_FORMAT, typename T>
struct DeserializeImpl<json::JSONParser<JSON_FORMAT>, T, std::enable_if_t<IS_CURRENT_VARIANT(T)>> {
  static void DoDeserialize(json::JSONParser<JSON_FORMAT>& json_parser, T& value) {
    if (!json_parser || json_parser.Current().IsNull()) {
      if (json::JSONVariantStyleUseNulls<JSON_FORMAT::variant_style>::value) {
        CURRENT_THROW(JSONUninitializedVariantObjectException());
      }
    } else {
      json::JSONVariantPerStyle<JSON_FORMAT::variant_style, JSON_FORMAT, T>::Instance().DoLoadVariant(json_parser,
                                                                                                      value);
    }
  }
};

}  // namespace current::serialization
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SERIALIZATION_JSON_VARIANT_H
