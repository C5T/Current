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

struct ReflectorImpl {
  static ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

  struct FieldReflector {
    FieldReflector(StructFieldsVector& fields) : fields_(fields) {}

    template <typename T>
    void operator()(TypeSelector<T>, const std::string& name) const {
      fields_.emplace_back(Reflector().ReflectType<T>(), name);
    }

   private:
    StructFieldsVector& fields_;
  };

  struct TypeReflector {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type) \
  std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<cpp_type>) {           \
    return std::make_shared<ReflectedType_##current_type>();                        \
  }
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

    template <typename T>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::vector<T>>) {
      auto v = std::make_shared<ReflectedType_Vector>(Reflector().ReflectType<T>());
      v->type_id = CalculateTypeID(v);
      return v;
    }

    template <typename TF, typename TS>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::pair<TF, TS>>) {
      auto p =
          std::make_shared<ReflectedType_Pair>(Reflector().ReflectType<TF>(), Reflector().ReflectType<TS>());
      p->type_id = CalculateTypeID(p);
      return p;
    }

    template <typename TK, typename TV>
    std::shared_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::map<TK, TV>>) {
      auto v =
          std::make_shared<ReflectedType_Map>(Reflector().ReflectType<TK>(), Reflector().ReflectType<TV>());
      v->type_id = CalculateTypeID(v);
      return v;
    }

    template <typename T>
    typename std::enable_if<std::is_base_of<CurrentSuper, T>::value, std::shared_ptr<ReflectedTypeImpl>>::type
    operator()(TypeSelector<T>) {
      auto s = std::make_shared<ReflectedType_Struct>();
      s->name = StructName<T>();
      s->reflected_super = ReflectSuper<T>();
      FieldReflector field_reflector(s->fields);
      VisitAllFields<T, FieldTypeAndName>::WithoutObject(field_reflector);
      s->type_id = CalculateTypeID(s);
      return s;
    }

   private:
    template <typename T>
    typename std::enable_if<std::is_same<SuperType<T>, CurrentSuper>::value,
                            std::shared_ptr<ReflectedType_Struct>>::type
    ReflectSuper() {
      return std::shared_ptr<ReflectedType_Struct>();
    }

    template <typename T>
    typename std::enable_if<!std::is_same<SuperType<T>, CurrentSuper>::value,
                            std::shared_ptr<ReflectedType_Struct>>::type
    ReflectSuper() {
      return std::dynamic_pointer_cast<ReflectedType_Struct>(Reflector().ReflectType<SuperType<T>>());
    }
  };

  template <typename T>
  std::shared_ptr<ReflectedTypeImpl> ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    auto& placeholder = reflected_types_[type_index];
    if (!placeholder) {
      placeholder = std::move(type_reflector_(TypeSelector<T>()));
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
