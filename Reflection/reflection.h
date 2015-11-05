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
    // TODO(dkorolev): Unify this part.
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<uint64_t>) {
      return make_unique<ReflectedType_UInt64>();
    }
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::string>) {
      return make_unique<ReflectedType_String>();
    }

    template <typename T>
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::vector<T>>) {
      auto v = make_unique<ReflectedType_Vector>();
      v->reflected_element_type = Reflector().ReflectType<T>();
      v->type_id = CalculateTypeID(v);
      return std::move(v);
    }

    template <typename T>
    typename std::enable_if<std::is_same<SuperType<T>, CurrentSuper>::value, std::string>::type
    StructInheritanceString() {
      return "";
    }

    template <typename T>
    typename std::enable_if<!std::is_same<SuperType<T>, CurrentSuper>::value, std::string>::type
    StructInheritanceString() {
      return std::string(" : ") + SuperType<T>::template CURRENT_REFLECTION_HELPER<SuperType<T>>::name();
    }

    template <typename T>
    typename std::enable_if<std::is_base_of<CurrentSuper, T>::value, std::unique_ptr<ReflectedTypeImpl>>::type
    operator()(TypeSelector<T>) {
      auto s = make_unique<ReflectedType_Struct>();
      s->name = T::template CURRENT_REFLECTION_HELPER<T>::name();
      s->super_name = StructInheritanceString<T>();
      FieldReflector field_reflector(s->fields);
      EnumFields<T, FieldTypeAndName>()(field_reflector);
      s->type_id = CalculateTypeID(s);
      return std::move(s);
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
