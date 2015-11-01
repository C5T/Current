#ifndef BRICKS_REFLECTION_TYPES_H
#define BRICKS_REFLECTION_TYPES_H

#include <string>
#include <vector>

#include "base.h"

enum class TypeID : uint64_t {
  UInt8 = 9000000000000000011u,
  UInt16 = 9000000000000000012u,
  UInt32 = 9000000000000000013u,
  UInt64 = 9000000000000000014u,
  Int8 = 9000000000000000021u,
  Int16 = 9000000000000000022u,
  Int32 = 9000000000000000023u,
  Int64 = 9000000000000000024u,
  Float = 9000000000000000031u,
  Double = 9000000000000000032u
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

#endif  // BRICKS_REFLECTION_TYPES_H
