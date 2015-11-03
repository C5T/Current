#ifndef BRICKS_REFLECTION_TYPES_H
#define BRICKS_REFLECTION_TYPES_H

#include <cassert>
#include <string>
#include <sstream>
#include <vector>

#include "base.h"
#include "crc32.h"

namespace bricks {
namespace reflection {

constexpr uint64_t TYPEID_TYPE_RANGE = static_cast<uint64_t>(1e16);
constexpr uint64_t TYPEID_BASIC_TYPE = 900u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_COLLECTION_TYPE = 901u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_STRUCT_TYPE = 902u * TYPEID_TYPE_RANGE;

enum class TypeID : uint64_t {
  UInt8 = TYPEID_BASIC_TYPE + 11,
  UInt16 = TYPEID_BASIC_TYPE + 12,
  UInt32 = TYPEID_BASIC_TYPE + 13,
  UInt64 = TYPEID_BASIC_TYPE + 14,
  Int8 = TYPEID_BASIC_TYPE + 21,
  Int16 = TYPEID_BASIC_TYPE + 22,
  Int32 = TYPEID_BASIC_TYPE + 23,
  Int64 = TYPEID_BASIC_TYPE + 24,
  Float = TYPEID_BASIC_TYPE + 31,
  Double = TYPEID_BASIC_TYPE + 32
};

struct ReflectedTypeImpl {
  TypeID type_id;
  virtual std::string CppType() = 0;
  virtual ~ReflectedTypeImpl() = default;
};

struct ReflectedType_UInt64 : ReflectedTypeImpl {
  TypeID type_id = TypeID::UInt64;
  std::string CppType() override { return "uint64_t"; }
};

struct ReflectedType_Vector : ReflectedTypeImpl {
  constexpr static const char* cpp_typename = "std::vector";
  ReflectedTypeImpl* reflected_element_type;

  std::string CppType() override {
    assert(reflected_element_type);
    std::ostringstream oss;
    oss << cpp_typename << "<" << reflected_element_type->CppType() << ">";
    return oss.str();
  }
};

typedef std::vector<std::pair<ReflectedTypeImpl*, std::string>> StructFieldsVector;

struct ReflectedType_Struct : ReflectedTypeImpl {
  std::string name;
  StructFieldsVector fields;

  std::string CppType() override { return name; }

  std::string CppDeclaration() {
    std::string result = "struct " + name + " {\n";
    for (const auto& f : fields) {
      result += "  " + f.first->CppType() + " " + f.second + ";\n";
    }
    result += "};\n";
    return result;
  }
};

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = crc32(s.name);
  size_t i = 0u;
  for (const auto& f : s.fields) {
    hash ^= (static_cast<uint64_t>(crc32(f.second)) << 8) ^
            (static_cast<uint64_t>(crc32(f.first->CppType())) << (16 + (i++ % 17)));
  }
  return static_cast<TypeID>(TYPEID_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Vector& v) {
  return static_cast<TypeID>(TYPEID_COLLECTION_TYPE + crc32(v.reflected_element_type->CppType()));
}

}  // namespace reflection
}  // namespace bricks

#endif  // BRICKS_REFLECTION_TYPES_H
