#ifndef BRICKS_REFLECTION_REFLECTION_H
#define BRICKS_REFLECTION_REFLECTION_H

#include <sstream>

#include "struct.h"
#include "types.h"

namespace bricks {
namespace reflection {

struct ReflectedField {
  ReflectedField(const std::string& name, std::unique_ptr<ReflectedTypeImpl> reflected_type)
      : name(name), reflected_type(std::move(reflected_type)) {}

  std::string name;
  std::unique_ptr<ReflectedTypeImpl> reflected_type;
};

struct ReflectedStruct {
  std::string name;
  std::vector<ReflectedField> fields;
};

struct TypeReflector {
  std::unique_ptr<ReflectedTypeImpl> operator()(TypeWrapper<uint64_t>) {
    return std::unique_ptr<ReflectedTypeImpl>(new ReflectedType_UInt64());
  }
};

struct FieldReflector {
  std::vector<ReflectedField> fields;
  TypeReflector type_reflector;

  template <typename T>
  void operator()(TypeWrapper<T> type_wrapper, const std::string& name) {
    fields.emplace_back(name, type_reflector(type_wrapper));
  }
};

template <typename T>
typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value, ReflectedStruct>::type ReflectStruct() {
  FieldReflector field_reflector;
  ReflectedStruct result;
  result.name = T::template CURRENT_REFLECTION_HELPER<T>::name();
  EnumFields<T, FieldTypeAndName>()(field_reflector);
  result.fields = std::move(field_reflector.fields);
  return result;
}

template <typename T>
typename std::enable_if<std::is_base_of<CurrentBaseType, T>::value, std::string>::type DescribeCppStruct() {
  std::ostringstream oss;
  auto r = ReflectStruct<T>();
  oss << "struct " << r.name << " {\n";
  for (const auto& f : r.fields) {
    oss << "  " << f.reflected_type->CppType() << " " << f.name << ";\n";
  }
  oss << "};\n";
  return oss.str();
}

}  // namespace reflection
}  // namespace bricks

#endif  // BRICKS_REFLECTION_REFLECTION_H
