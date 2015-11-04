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
  struct FieldReflector {
    FieldReflector(StructFieldsVector& fields) : fields_(fields) {}

    ReflectorImpl& Reflector() const { return ThreadLocalSingleton<ReflectorImpl>(); }

    template <typename T>
    void operator()(TypeSelector<T>, const std::string& name) const {
      fields_.emplace_back(Reflector().ReflectType<T>(), name);
    }

   private:
    StructFieldsVector& fields_;
  };

  struct TypeReflector {
    ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<uint64_t>) {
      return std::unique_ptr<ReflectedTypeImpl>(new ReflectedType_UInt64());
    }

    template <typename T>
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeSelector<std::vector<T>>) {
      std::unique_ptr<ReflectedTypeImpl> result(new ReflectedType_Vector());
      ReflectedType_Vector& v = dynamic_cast<ReflectedType_Vector&>(*result);
      v.reflected_element_type = Reflector().ReflectType<T>();
      v.type_id = CalculateTypeID(v);
      return result;
    }

    template <typename T>
    typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value,
                            std::unique_ptr<ReflectedTypeImpl>>::type
    operator()(TypeSelector<T>) {
      std::unique_ptr<ReflectedTypeImpl> result(new ReflectedType_Struct());
      ReflectedType_Struct& s = dynamic_cast<ReflectedType_Struct&>(*result);
      s.name = T::template CURRENT_REFLECTION_HELPER<T>::name();
      FieldReflector field_reflector(s.fields);
      EnumFields<T, FieldTypeAndName>()(field_reflector);
      s.type_id = CalculateTypeID(s);
      return result;
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
  typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value, std::string>::type DescribeCppStruct() {
    return ReflectType<T>()->CppDeclaration();
  }

  size_t KnownTypesCountForUnitTest() const { return reflected_types_.size(); }

 private:
  std::unordered_map<std::type_index, std::unique_ptr<ReflectedTypeImpl>> reflected_types_;
  TypeReflector type_reflector_;
};

inline ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_REFLECTION_REFLECTION_H
