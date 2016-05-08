/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H

#include <typeindex>
#include <unordered_map>

#include "exceptions.h"
#include "types.h"

#include "../optional.h"
#include "../variant.h"
#include "../struct.h"
#include "../timestamp.h"

#include "../../Bricks/util/comparators.h"
#include "../../Bricks/util/singleton.h"

namespace current {
namespace reflection {

using current::ThreadLocalSingleton;

struct TypeReflector;

// `ReflectorImpl` is a thread-local singleton to generate reflected types metadata at runtime.
struct ReflectorImpl {
  static ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

  template <typename TOP_LEVEL>
  struct StructFieldReflector {
    using fields_list_t = std::vector<ReflectedType_Struct_Field>;

    StructFieldReflector(fields_list_t& fields, bool reflect_names_only)
        : fields_(fields), reflect_names_only_(reflect_names_only) {}

    template <typename T, int I>
    void operator()(TypeSelector<T>, const std::string& name, SimpleIndex<I>) const {
      const char* retrieved_description = FieldDescriptions::template Description<TOP_LEVEL, I>();
      Optional<std::string> description;
      if (retrieved_description) {
        description = retrieved_description;
      }
      if (reflect_names_only_) {
        fields_.emplace_back(TypeID::INVALID_TYPE, name, description);
      } else {
        fields_.emplace_back(Value<ReflectedTypeBase>(Reflector().ReflectType<T>()).type_id, name, description);
      }
    }

   private:
    fields_list_t& fields_;
    const bool reflect_names_only_;
  };

  struct TypeReflector {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type) \
  ReflectedType operator()(TypeSelector<cpp_type>) {                                           \
    return ReflectedType(ReflectedType_Primitive(TypeID::current_type));                       \
  }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

    template <typename T>
    ENABLE_IF<std::is_enum<T>::value, ReflectedType> operator()(TypeSelector<T>) {
      return ReflectedType(ReflectedType_Enum(
          EnumName<T>(),
          Value<ReflectedTypeBase>(Reflector().ReflectType<typename std::underlying_type<T>::type>()).type_id));
    }

    template <typename T>
    ReflectedType operator()(TypeSelector<std::vector<T>>) {
      ReflectedType_Vector result(Value<ReflectedTypeBase>(Reflector().ReflectType<T>()).type_id);
      result.type_id = CalculateTypeID(result);
      return ReflectedType(std::move(result));
    }

    template <typename TF, typename TS>
    ReflectedType operator()(TypeSelector<std::pair<TF, TS>>) {
      ReflectedType_Pair result(Value<ReflectedTypeBase>(Reflector().ReflectType<TF>()).type_id,
                                Value<ReflectedTypeBase>(Reflector().ReflectType<TS>()).type_id);
      result.type_id = CalculateTypeID(result);
      return ReflectedType(std::move(result));
    }

    template <typename TK, typename TV>
    ReflectedType operator()(TypeSelector<std::map<TK, TV>>) {
      ReflectedType_Map result(Value<ReflectedTypeBase>(Reflector().ReflectType<TK>()).type_id,
                               Value<ReflectedTypeBase>(Reflector().ReflectType<TV>()).type_id);
      result.type_id = CalculateTypeID(result);
      return ReflectedType(std::move(result));
    }

    template <typename T>
    ReflectedType operator()(TypeSelector<Optional<T>>) {
      ReflectedType_Optional result(Value<ReflectedTypeBase>(Reflector().ReflectType<T>()).type_id);
      result.type_id = CalculateTypeID(result);
      return ReflectedType(std::move(result));
    }

    template <typename... TS>
    struct VisitAllVariantTypes {
      template <typename X>
      struct VisitImpl {
        static void DispatchToAll(ReflectedType_Variant& destination) {
          destination.cases.push_back(Value<ReflectedTypeBase>(Reflector().ReflectType<X>()).type_id);
        }
      };
      static void Run(ReflectedType_Variant& destination) {
        current::metaprogramming::combine<current::metaprogramming::map<VisitImpl, TypeListImpl<TS...>>> impl;
        impl.DispatchToAll(destination);
      }
    };

    template <typename NAME, typename... TS>
    ReflectedType operator()(TypeSelector<VariantImpl<NAME, TypeListImpl<TS...>>>) {
      ReflectedType_Variant result;
      result.name = NAME::VariantNameImpl();
      VisitAllVariantTypes<TS...>::Run(result);
      result.type_id = CalculateTypeID(result);
      return ReflectedType(std::move(result));
    }

    template <typename T>
    ENABLE_IF<IS_CURRENT_STRUCT(T), void> operator()(TypeSelector<T>, ReflectedType_Struct& s) {
      // Two step reflection is needed to support self-referring structs.
      if (TypePrefix(s.type_id) != TYPEID_INCOMPLETE_STRUCT_PREFIX) {
        // First step: incomplete struct reflection using only field names.
        StructFieldReflector<T> field_names_reflector(s.fields, true);
        VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(field_names_reflector);
        s.name = CurrentTypeName<T>();
        s.type_id = CalculateTypeID(s, true);
        const TypeID incomplete_id = s.type_id;

        // Second step: full reflection with field types and names.
        s.super_id = ReflectSuper<T>();
        s.fields.clear();
        StructFieldReflector<T> field_reflector(s.fields, false);
        VisitAllFields<T, FieldTypeAndNameAndIndex>::WithoutObject(field_reflector);
        s.type_id = CalculateTypeID(s);
        Reflector().FixIncompleteTypeIDs(incomplete_id, s.type_id);
      }
    }

   private:
    template <typename T>
    ENABLE_IF<std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
      return TypeID::CurrentStruct;
    }

    template <typename T>
    ENABLE_IF<!std::is_same<SuperType<T>, CurrentStruct>::value, TypeID> ReflectSuper() {
      return Value<ReflectedTypeBase>(Reflector().ReflectType<SuperType<T>>()).type_id;
    }
  };

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT(T), const ReflectedType&> ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    if (!reflected_cpp_types_.count(type_index)) {
      reflected_cpp_types_.insert(std::make_pair(type_index, type_reflector_(TypeSelector<T>())));
      type_index_by_type_id_[Value<ReflectedTypeBase>(reflected_cpp_types_.at(type_index)).type_id] =
          type_index;
    }
    return reflected_cpp_types_.at(type_index);
  }

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT(T), const ReflectedType&> ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    if (!reflected_cpp_types_.count(type_index)) {
      reflected_cpp_types_.emplace(type_index, ReflectedType_Struct());
      type_reflector_(TypeSelector<T>(), Value<ReflectedType_Struct>(reflected_cpp_types_.at(type_index)));
      type_index_by_type_id_[Value<ReflectedTypeBase>(reflected_cpp_types_.at(type_index)).type_id] =
          type_index;
    }
    return reflected_cpp_types_.at(type_index);
  }

  const ReflectedType& ReflectedTypeByTypeID(const TypeID type_id) const {
    const auto cit = type_index_by_type_id_.find(type_id);
    if (cit != type_index_by_type_id_.end()) {
      return reflected_cpp_types_.at(cit->second.type_index);
    } else {
      throw UnknownTypeIDException();  // LCOV_EXCL_LINE
    }
  }

  size_t KnownTypesCountForUnitTest() const { return reflected_cpp_types_.size(); }

 private:
  std::unordered_map<std::type_index, ReflectedType> reflected_cpp_types_;
  struct DefaultConstructibleTypeIndex {
    std::type_index type_index = std::type_index(typeid(void));
    void operator=(const std::type_index new_value) { type_index = new_value; }
  };
  std::unordered_map<TypeID, DefaultConstructibleTypeIndex, CurrentHashFunction<TypeID>> type_index_by_type_id_;
  TypeReflector type_reflector_;

  void FixIncompleteTypeIDs(const TypeID incomplete_id, const TypeID real_id) {
    struct Impl {
      const TypeID from_;
      const TypeID to_;
      Impl(TypeID from, TypeID to) : from_(from), to_(to) {}

      void operator()(ReflectedType_Struct& s) const {
        if (s.super_id == from_) {
          s.super_id = to_;
        }
        for (auto& f : s.fields) {
          if (f.type_id == from_) {
            f.type_id = to_;
          }
        }
      }

      void operator()(ReflectedType_Primitive&) const {}

      void operator()(ReflectedType_Enum&) const {}

      void operator()(ReflectedType_Vector& v) const {
        if (v.element_type == from_) {
          v.element_type = to_;
        }
      }

      void operator()(ReflectedType_Map& m) const {
        if (m.key_type == from_) {
          m.key_type = to_;
        }
        if (m.value_type == from_) {
          m.value_type = to_;
        }
      }

      void operator()(ReflectedType_Pair& p) const {
        if (p.first_type == from_) {
          p.first_type = to_;
        }
        if (p.second_type == from_) {
          p.second_type = to_;
        }
      }

      void operator()(ReflectedType_Optional& o) const {
        if (o.optional_type == from_) {
          o.optional_type = to_;
        }
      }

      void operator()(ReflectedType_Variant& p) const {
        for (auto& c : p.cases) {
          if (c == from_) {
            c = to_;
          }
        }
      }
    };

    Impl impl(incomplete_id, real_id);
    for (auto& t : reflected_cpp_types_) {
      t.second.Call(impl);
    }
  }
};

inline ReflectorImpl& Reflector() { return ReflectorImpl::Reflector(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
