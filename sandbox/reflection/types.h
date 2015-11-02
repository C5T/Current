#ifndef BRICKS_REFLECTION_TYPES_H
#define BRICKS_REFLECTION_TYPES_H

#include <string>
#include <vector>

#include "base.h"

namespace bricks {
namespace reflection {

constexpr uint64_t TYPEID_BASIC_TYPE = 9000000000000000000u;

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

}  // namespace reflection
}  // namespace bricks

#endif  // BRICKS_REFLECTION_TYPES_H
