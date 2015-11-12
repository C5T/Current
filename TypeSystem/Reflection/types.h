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
#include "../struct.h"

#include "../../Bricks/util/crc32.h"

namespace current {
namespace reflection {

// clang-format off
constexpr uint64_t TYPEID_BASIC_PREFIX  = 900u;
constexpr uint64_t TYPEID_STRUCT_PREFIX = 920u;
constexpr uint64_t TYPEID_VECTOR_PREFIX = 931u;
constexpr uint64_t TYPEID_SET_PREFIX    = 932u;
constexpr uint64_t TYPEID_PAIR_PREFIX   = 933u;
constexpr uint64_t TYPEID_MAP_PREFIX    = 934u;

constexpr uint64_t TYPEID_TYPE_RANGE  = static_cast<uint64_t>(1e16);
constexpr uint64_t TYPEID_BASIC_TYPE  = TYPEID_BASIC_PREFIX * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_STRUCT_TYPE = TYPEID_STRUCT_PREFIX * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_VECTOR_TYPE = TYPEID_VECTOR_PREFIX * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_SET_TYPE    = TYPEID_SET_PREFIX * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_PAIR_TYPE   = TYPEID_PAIR_PREFIX * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_MAP_TYPE    = TYPEID_MAP_PREFIX * TYPEID_TYPE_RANGE;
// clang-format on

enum class TypeID : uint64_t {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, unused_cpp_type, current_type) \
  current_type = TYPEID_BASIC_TYPE + typeid_index,
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  INVALID_TYPE = 0u
};

inline uint64_t TypePrefix(const uint64_t type_id) { return type_id / TYPEID_TYPE_RANGE; }

inline uint64_t TypePrefix(const TypeID type_id) { return TypePrefix(static_cast<uint64_t>(type_id)); }

struct ReflectedTypeImpl {
  TypeID type_id = TypeID::INVALID_TYPE;
  virtual ~ReflectedTypeImpl() = default;
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type) \
  struct ReflectedType_##current_type : ReflectedTypeImpl {                         \
    ReflectedType_##current_type() { type_id = TypeID::current_type; }              \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

struct ReflectedType_Vector : ReflectedTypeImpl {
  ReflectedTypeImpl* reflected_element_type;
};

struct ReflectedType_Map : ReflectedTypeImpl {
  ReflectedTypeImpl* reflected_key_type;
  ReflectedTypeImpl* reflected_value_type;
};

struct ReflectedType_Pair : ReflectedTypeImpl {
  ReflectedTypeImpl* reflected_first_type;
  ReflectedTypeImpl* reflected_second_type;
};

typedef std::vector<std::pair<ReflectedTypeImpl*, std::string>> StructFieldsVector;

struct ReflectedType_Struct : ReflectedTypeImpl {
  std::string name;
  ReflectedType_Struct* reflected_super = nullptr;
  StructFieldsVector fields;
};

inline uint64_t rol64(const uint64_t value, size_t nbits) {
  nbits &= 63;
  return (value << nbits) | (value >> (-nbits & 63));
}

inline uint64_t rol64(const TypeID type_id, size_t nbits) {
  return rol64(static_cast<uint64_t>(type_id), nbits);
}

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = bricks::CRC32(s.name);
  size_t i = 0u;
  for (const auto& f : s.fields) {
    assert(f.first);
    assert(f.first->type_id != TypeID::INVALID_TYPE);
    hash ^= rol64(f.first->type_id, i + 8) ^ rol64(bricks::CRC32(f.second), i + 16);
    ++i;
  }
  return static_cast<TypeID>(TYPEID_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Vector& v) {
  assert(v.reflected_element_type);
  assert(v.reflected_element_type->type_id != TypeID::INVALID_TYPE);
  return static_cast<TypeID>(TYPEID_VECTOR_TYPE +
                             rol64(v.reflected_element_type->type_id, 1) % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Pair& v) {
  assert(v.reflected_first_type);
  assert(v.reflected_second_type);
  assert(v.reflected_first_type->type_id != TypeID::INVALID_TYPE);
  assert(v.reflected_second_type->type_id != TypeID::INVALID_TYPE);
  uint64_t hash = rol64(v.reflected_first_type->type_id, 4) ^ rol64(v.reflected_second_type->type_id, 8);
  return static_cast<TypeID>(TYPEID_PAIR_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Map& v) {
  assert(v.reflected_key_type);
  assert(v.reflected_value_type);
  assert(v.reflected_key_type->type_id != TypeID::INVALID_TYPE);
  assert(v.reflected_value_type->type_id != TypeID::INVALID_TYPE);
  uint64_t hash = rol64(v.reflected_key_type->type_id, 4) ^ rol64(v.reflected_value_type->type_id, 8);
  return static_cast<TypeID>(TYPEID_MAP_TYPE + hash % TYPEID_TYPE_RANGE);
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
