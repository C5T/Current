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

#include "types.h"

#include "../struct.h"

#include "../../Bricks/util/singleton.h"

namespace current {
namespace reflection {

using bricks::ThreadLocalSingleton;

struct TypeReflector;

// `ReflectorImpl` is a thread-local singleton to generate reflected types metadata at runtime.
struct ReflectorImpl {
  static ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

  struct FieldReflector {
    FieldReflector(StructFieldsVector& fields, bool reflect_names_only)
        : fields_(fields), reflect_names_only_(reflect_names_only) {}

    template <typename T>
    void operator()(TypeSelector<T>, const std::string& name) const {
      if (reflect_names_only_) {
        fields_.emplace_back(nullptr, name);
      } else {
        fields_.emplace_back(Reflector().ReflectType<T>(), name);
      }
    }

   private:
    StructFieldsVector& fields_;
    const bool reflect_names_only_;
  };

  struct TypeReflector {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type) \
  std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<cpp_type>) {           \
    return std::make_shared<ReflectedType_##current_type>();                        \
  }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

    template <typename T>
    ENABLE_IF<std::is_enum<T>::value, std::shared_ptr<ReflectedTypeImpl>> operator()(TypeSelector<T>) {
      return std::make_shared<ReflectedType_Enum>(
          T(), Reflector().ReflectType<typename std::underlying_type<T>::type>());
    }

    template <typename T>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::vector<T>>) {
      auto v = std::make_shared<ReflectedType_Vector>(Reflector().ReflectType<T>());
      v->type_id = CalculateTypeID(*v);
      return v;
    }

    template <typename TF, typename TS>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::pair<TF, TS>>) {
      auto p =
          std::make_shared<ReflectedType_Pair>(Reflector().ReflectType<TF>(), Reflector().ReflectType<TS>());
      p->type_id = CalculateTypeID(*p);
      return p;
    }

    template <typename TK, typename TV>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::map<TK, TV>>) {
      auto m =
          std::make_shared<ReflectedType_Map>(Reflector().ReflectType<TK>(), Reflector().ReflectType<TV>());
      m->type_id = CalculateTypeID(*m);
      return m;
    }

    template <typename T>
    ENABLE_IF<IS_CURRENT_STRUCT(T), std::shared_ptr<ReflectedTypeImpl>> operator()(
        TypeSelector<T>, std::shared_ptr<ReflectedType_Struct> s) {
      // Two step reflection is needed to support self-referring structs.
      if (TypePrefix(s->type_id) != TYPEID_INCOMPLETE_STRUCT_PREFIX) {
        // First step: incomplete struct reflection using only field names.
        FieldReflector field_names_reflector(s->fields, true);
        VisitAllFields<T, FieldTypeAndName>::WithoutObject(field_names_reflector);
        s->name = StructName<T>();
        s->type_id = CalculateTypeID(*s, true);

        // Second step: full reflection with field types and names.
        s->reflected_super = ReflectSuper<T>();
        s->fields.clear();
        FieldReflector field_reflector(s->fields, false);
        VisitAllFields<T, FieldTypeAndName>::WithoutObject(field_reflector);
        s->type_id = CalculateTypeID(*s);
      }
      return s;
    }

   private:
    template <typename T>
    ENABLE_IF<std::is_same<SuperType<T>, CurrentSuper>::value, std::shared_ptr<ReflectedType_Struct>>
    ReflectSuper() {
      return std::shared_ptr<ReflectedType_Struct>();
    }

    template <typename T>
    ENABLE_IF<!std::is_same<SuperType<T>, CurrentSuper>::value, std::shared_ptr<ReflectedType_Struct>>
    ReflectSuper() {
      return std::dynamic_pointer_cast<ReflectedType_Struct>(Reflector().ReflectType<SuperType<T>>());
    }
  };

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT(T), std::shared_ptr<ReflectedTypeImpl>> ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    auto& placeholder = reflected_types_[type_index];
    if (!placeholder) {
      placeholder = type_reflector_(TypeSelector<T>());
    }
    return placeholder;
  }

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT(T), std::shared_ptr<ReflectedTypeImpl>> ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    auto& placeholder = reflected_types_[type_index];
    if (!placeholder) {
      placeholder = std::make_shared<ReflectedType_Struct>();
      type_reflector_(TypeSelector<T>(), std::dynamic_pointer_cast<ReflectedType_Struct>(placeholder));
    }
    return placeholder;
  }

  size_t KnownTypesCountForUnitTest() const { return reflected_types_.size(); }

 private:
  std::unordered_map<std::type_index, std::shared_ptr<ReflectedTypeImpl>> reflected_types_;
  TypeReflector type_reflector_;
};

inline ReflectorImpl& Reflector() { return ReflectorImpl::Reflector(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_REFLECTION_H
