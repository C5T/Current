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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_TYPES_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_TYPES_H

#include <cassert>
#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <memory>

#include "../base.h"
#include "../enum.h"
#include "../struct.h"

#include "../../Bricks/template/enable_if.h"
#include "../../Bricks/util/crc32.h"
#include "../../Bricks/util/rol.h"

namespace current {
namespace reflection {

// clang-format off
// Special type prefix for structs containing a reference to itself.
constexpr uint64_t TYPEID_INCOMPLETE_STRUCT_PREFIX = 800u;
// Basic types prefixes.
constexpr uint64_t TYPEID_BASIC_PREFIX = 900u;
constexpr uint64_t TYPEID_ENUM_PREFIX  = 901u;
// Current complex types prefixes.
constexpr uint64_t TYPEID_STRUCT_PREFIX      = 920u;
constexpr uint64_t TYPEID_OPTIONAL_PREFIX    = 921u;
constexpr uint64_t TYPEID_POLYMORPHIC_PREFIX = 922u;
// STL containers prefixes.
constexpr uint64_t TYPEID_VECTOR_PREFIX = 931u;
constexpr uint64_t TYPEID_SET_PREFIX    = 932u;
constexpr uint64_t TYPEID_PAIR_PREFIX   = 933u;
constexpr uint64_t TYPEID_MAP_PREFIX    = 934u;

// Range of possible TypeID-s for each prefix.
constexpr uint64_t TYPEID_TYPE_RANGE = static_cast<uint64_t>(1e16);
// Base TypeID for structs containing a reference to itself.
constexpr uint64_t TYPEID_INCOMPLETE_STRUCT_TYPE = TYPEID_TYPE_RANGE * TYPEID_INCOMPLETE_STRUCT_PREFIX;
// Base TypeID-s for basic types.
constexpr uint64_t TYPEID_BASIC_TYPE = TYPEID_TYPE_RANGE * TYPEID_BASIC_PREFIX;
constexpr uint64_t TYPEID_ENUM_TYPE  = TYPEID_TYPE_RANGE * TYPEID_ENUM_PREFIX;
// Base TypeID-s for Current complex types.
constexpr uint64_t TYPEID_STRUCT_TYPE      = TYPEID_TYPE_RANGE * TYPEID_STRUCT_PREFIX;
constexpr uint64_t TYPEID_OPTIONAL_TYPE    = TYPEID_TYPE_RANGE * TYPEID_OPTIONAL_PREFIX;
constexpr uint64_t TYPEID_POLYMORPHIC_TYPE = TYPEID_TYPE_RANGE * TYPEID_POLYMORPHIC_PREFIX;
// Base TypeID-s for STL containers.
constexpr uint64_t TYPEID_VECTOR_TYPE = TYPEID_TYPE_RANGE * TYPEID_VECTOR_PREFIX;
constexpr uint64_t TYPEID_SET_TYPE    = TYPEID_TYPE_RANGE * TYPEID_SET_PREFIX;
constexpr uint64_t TYPEID_PAIR_TYPE   = TYPEID_TYPE_RANGE * TYPEID_PAIR_PREFIX;
constexpr uint64_t TYPEID_MAP_TYPE    = TYPEID_TYPE_RANGE * TYPEID_MAP_PREFIX;
// clang-format on

CURRENT_ENUM(TypeID, uint64_t){
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, unused_cpp_type, current_type, unused_fsharp_type) \
  current_type = TYPEID_BASIC_TYPE + typeid_index,
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
    INVALID_TYPE = 0u};

inline uint64_t TypePrefix(const uint64_t type_id) { return type_id / TYPEID_TYPE_RANGE; }

inline uint64_t TypePrefix(const TypeID type_id) { return TypePrefix(static_cast<uint64_t>(type_id)); }

struct ReflectedTypeImpl {
  TypeID type_id = TypeID::INVALID_TYPE;
  virtual ~ReflectedTypeImpl() = default;
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type, unused_fsharp_type) \
  struct ReflectedType_##current_type : ReflectedTypeImpl {                                             \
    ReflectedType_##current_type() { type_id = TypeID::current_type; }                                  \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

struct ReflectedType_Enum : ReflectedTypeImpl {
  const std::string name;
  const std::shared_ptr<ReflectedTypeImpl> reflected_underlying_type;
  template <typename T>
  ReflectedType_Enum(T, const std::shared_ptr<ReflectedTypeImpl> rt)
      : name(EnumName<T>()), reflected_underlying_type(rt) {
    type_id = static_cast<TypeID>(TYPEID_ENUM_TYPE + bricks::CRC32(name));
  }
};

struct ReflectedType_Vector : ReflectedTypeImpl {
  const std::shared_ptr<ReflectedTypeImpl> reflected_element_type;
  ReflectedType_Vector(const std::shared_ptr<ReflectedTypeImpl> re) : reflected_element_type(re) {}
};

struct ReflectedType_Map : ReflectedTypeImpl {
  const std::shared_ptr<ReflectedTypeImpl> reflected_key_type;
  const std::shared_ptr<ReflectedTypeImpl> reflected_value_type;
  ReflectedType_Map(const std::shared_ptr<ReflectedTypeImpl> rk, const std::shared_ptr<ReflectedTypeImpl> rv)
      : reflected_key_type(rk), reflected_value_type(rv) {}
};

struct ReflectedType_Pair : ReflectedTypeImpl {
  std::shared_ptr<ReflectedTypeImpl> reflected_first_type;
  std::shared_ptr<ReflectedTypeImpl> reflected_second_type;
  ReflectedType_Pair(const std::shared_ptr<ReflectedTypeImpl> rf, const std::shared_ptr<ReflectedTypeImpl> rs)
      : reflected_first_type(rf), reflected_second_type(rs) {}
};

struct ReflectedType_Optional : ReflectedTypeImpl {
  const std::shared_ptr<ReflectedTypeImpl> reflected_object_type;
  ReflectedType_Optional(const std::shared_ptr<ReflectedTypeImpl> re) : reflected_object_type(re) {}
};

typedef std::vector<std::pair<std::shared_ptr<ReflectedTypeImpl>, std::string>> StructFieldsVector;

struct ReflectedType_Struct : ReflectedTypeImpl {
  std::string name;
  std::shared_ptr<ReflectedType_Struct> reflected_super;
  StructFieldsVector fields;
};

inline uint64_t ROL64(const TypeID type_id, size_t nbits) {
  return current::ROL64(static_cast<uint64_t>(type_id), nbits);
}

inline TypeID CalculateTypeID(const ReflectedType_Struct& s, bool is_incomplete = false) {
  uint64_t hash = bricks::CRC32(s.name);
  size_t i = 0u;
  if (is_incomplete) {
    // For incomplete structs we use only the names of the fields for hashing,
    // since we don't know their real `type_id`-s.
    // At this moment, reflected types of the fields should be empty.
    for (const auto& f : s.fields) {
      assert(!f.first);
      hash ^= current::ROL64(bricks::CRC32(f.second), i + 19);
      ++i;
    }
    return static_cast<TypeID>(TYPEID_INCOMPLETE_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
  } else {
    for (const auto& f : s.fields) {
      assert(f.first);
      assert(f.first->type_id != TypeID::INVALID_TYPE);
      hash ^= ROL64(f.first->type_id, i + 17) ^ current::ROL64(bricks::CRC32(f.second), i + 29);
      ++i;
    }
    return static_cast<TypeID>(TYPEID_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
  }
}

inline TypeID CalculateTypeID(const ReflectedType_Vector& v) {
  assert(v.reflected_element_type);
  assert(v.reflected_element_type->type_id != TypeID::INVALID_TYPE);
  return static_cast<TypeID>(TYPEID_VECTOR_TYPE +
                             ROL64(v.reflected_element_type->type_id, 3) % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Pair& v) {
  assert(v.reflected_first_type);
  assert(v.reflected_second_type);
  assert(v.reflected_first_type->type_id != TypeID::INVALID_TYPE);
  assert(v.reflected_second_type->type_id != TypeID::INVALID_TYPE);
  uint64_t hash = ROL64(v.reflected_first_type->type_id, 5) ^ ROL64(v.reflected_second_type->type_id, 11);
  return static_cast<TypeID>(TYPEID_PAIR_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Map& v) {
  assert(v.reflected_key_type);
  assert(v.reflected_value_type);
  assert(v.reflected_key_type->type_id != TypeID::INVALID_TYPE);
  assert(v.reflected_value_type->type_id != TypeID::INVALID_TYPE);
  uint64_t hash = ROL64(v.reflected_key_type->type_id, 5) ^ ROL64(v.reflected_value_type->type_id, 11);
  return static_cast<TypeID>(TYPEID_MAP_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Optional& o) {
  assert(o.reflected_object_type);
  assert(o.reflected_object_type->type_id != TypeID::INVALID_TYPE);
  return static_cast<TypeID>(TYPEID_OPTIONAL_TYPE +
                             ROL64(o.reflected_object_type->type_id, 5) % TYPEID_TYPE_RANGE);
}

// Enable `CalculateTypeID` for bare and smart pointers.
template <typename T, typename DELETER>
TypeID CalculateTypeID(const std::unique_ptr<T, DELETER>& ptr) {
  return CalculateTypeID(*ptr);
}
template <typename T>
TypeID CalculateTypeID(const std::shared_ptr<T>& ptr) {
  return CalculateTypeID(*ptr);
}
template <typename T>
TypeID CalculateTypeID(const T* ptr) {
  return CalculateTypeID(*ptr);
}

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_TYPES_H
