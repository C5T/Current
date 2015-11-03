#ifndef BRICKS_REFLECTION_REFLECTION_H
#define BRICKS_REFLECTION_REFLECTION_H

#include <typeindex>
#include <unordered_map>

#include "struct.h"
#include "types.h"

#include "../../Bricks/util/singleton.h"

namespace bricks {
namespace reflection {

struct TypeReflector;

struct ReflectorImpl {
  struct FieldReflector {
    FieldReflector(StructFieldsVector& fields) : fields_(fields) {}

    template <typename T>
    void operator()(TypeWrapper<T>, const std::string& name) const {
      fields_.emplace_back(ThreadLocalSingleton<ReflectorImpl>().ReflectType<T>(), name);
    }

   private:
    StructFieldsVector& fields_;
  };

  struct TypeReflector {
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeWrapper<uint64_t>) {
      return std::unique_ptr<ReflectedTypeImpl>(new ReflectedType_UInt64());
    }

    template <typename T>
    std::unique_ptr<ReflectedTypeImpl> operator()(TypeWrapper<std::vector<T>>) {
      std::unique_ptr<ReflectedTypeImpl> result(new ReflectedType_Vector());
      ReflectedType_Vector& v = dynamic_cast<ReflectedType_Vector&>(*result);
      v.reflected_element_type = ThreadLocalSingleton<ReflectorImpl>().ReflectType<T>();
      v.type_id = CalculateTypeID(v);
      return result;
    }

    template <typename T>
    typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value,
                            std::unique_ptr<ReflectedTypeImpl>>::type
    operator()(TypeWrapper<T>) {
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
    std::type_index type_index = std::type_index(typeid(T));
    if (reflected_types_.count(type_index) == 0u) {
      reflected_types_[type_index] = std::move(type_reflector_(TypeWrapper<T>()));
    }
    return reflected_types_[type_index].get();
  }

  template <typename T>
  typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value, std::string>::type DescribeCppStruct() {
    return dynamic_cast<ReflectedType_Struct*>(ReflectType<T>())->CppDeclaration();
  }

  // For testing purposes.
  size_t KnownTypesCount() const { return reflected_types_.size(); }

 private:
  std::unordered_map<std::type_index, std::unique_ptr<ReflectedTypeImpl>> reflected_types_;
  TypeReflector type_reflector_;
};

inline ReflectorImpl& Reflector() { return ThreadLocalSingleton<ReflectorImpl>(); }

}  // namespace reflection
}  // namespace bricks

#endif  // BRICKS_REFLECTION_REFLECTION_H
