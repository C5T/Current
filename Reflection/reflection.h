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

#ifndef CURRENT_REFLECTION_REFLECTION_H
#define CURRENT_REFLECTION_REFLECTION_H

#include <typeindex>
#include <unordered_map>

#include "struct.h"
#include "types.h"

#include "../Bricks/util/singleton.h"

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
  std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<cpp_type>) {           \
    return make_unique<ReflectedType_##current_type>();                             \
  }
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

    template <typename T>
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::vector<T>>) {
      auto v = make_unique<ReflectedType_Vector>();
      v->reflected_element_type = Reflector().ReflectType<T>();
      v->type_id = CalculateTypeID(v);
      return std::move(v);
    }

    template <typename T>
    typename std::enable_if<std::is_base_of<CurrentSuper, T>::value, std::unique_ptr<ReflectedTypeImpl>>::type
    operator()(TypeSelector<T>) {
      auto s = make_unique<ReflectedType_Struct>();
      s->name = T::template CURRENT_REFLECTION_HELPER<T>::name();
      s->super_name = SuperNameIfNotCurrentSuper<T>();
      FieldReflector field_reflector(s->fields);
      VisitAllFields<T, FieldTypeAndName>::WithoutObject(field_reflector);
      s->type_id = CalculateTypeID(s);
      return std::move(s);
    }

   private:
    template <typename T>
    typename std::enable_if<std::is_same<SuperType<T>, CurrentSuper>::value, std::string>::type
    SuperNameIfNotCurrentSuper() {
      return "";
    }

    template <typename T>
    typename std::enable_if<!std::is_same<SuperType<T>, CurrentSuper>::value, std::string>::type
    SuperNameIfNotCurrentSuper() {
      return SuperType<T>::template CURRENT_REFLECTION_HELPER<SuperType<T>>::name();
    }
  };

  template <typename T>
  ReflectedTypeImpl* ReflectType() {
    const std::type_index type_index = std::type_index(typeid(T));
    auto& placeholder = reflected_types_[type_index];
    if (!placeholder) {
      placeholder = std::move(type_reflector_(TypeSelector<T>()));
    }
    return placeholder.get();
  }

  template <typename T>
  typename std::enable_if<std::is_base_of<CurrentSuper, T>::value, std::string>::type DescribeCppStruct() {
    return ReflectType<T>()->CppDeclaration();
  }

  size_t KnownTypesCountForUnitTest() const { return reflected_types_.size(); }

 private:
  std::unordered_map<std::type_index, std::unique_ptr<ReflectedTypeImpl>> reflected_types_;
  TypeReflector type_reflector_;
};

inline ReflectorImpl& Reflector() { return ReflectorImpl::Reflector(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_REFLECTION_REFLECTION_H
