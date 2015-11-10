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

constexpr uint64_t TYPEID_TYPE_RANGE = static_cast<uint64_t>(1e16);
constexpr uint64_t TYPEID_BASIC_TYPE = 900u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_STRUCT_TYPE = 920u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_VECTOR_TYPE = 931u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_SET_TYPE = 932u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_PAIR_TYPE = 933u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_MAP_TYPE = 934u * TYPEID_TYPE_RANGE;

enum class TypeID : uint64_t {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, unused_cpp_type, current_type) \
  current_type = TYPEID_BASIC_TYPE + typeid_index,
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  INVALID_TYPE = 0u
};

struct TypeIDHash {
  std::size_t operator()(const TypeID t) const { return static_cast<size_t>(t); }
};

inline bool IsTypeIDInRange(const TypeID type_id, const uint64_t id_range_base) {
  return (static_cast<uint64_t>(type_id) >= id_range_base &&
          static_cast<uint64_t>(type_id) < id_range_base + TYPEID_TYPE_RANGE);
}

struct ReflectedTypeImpl {
  TypeID type_id = TypeID::INVALID_TYPE;
  virtual std::string CppType() = 0;
  virtual std::string CppDeclaration() { return ""; }
  virtual ~ReflectedTypeImpl() = default;
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type) \
  struct ReflectedType_##current_type : ReflectedTypeImpl {                         \
    ReflectedType_##current_type() { type_id = TypeID::current_type; }              \
    std::string CppType() override { return #cpp_type; }                            \
  };
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

struct ReflectedType_Vector : ReflectedTypeImpl {
  constexpr static const char* cpp_typename = "std::vector";
  ReflectedTypeImpl* reflected_element_type;

  std::string CppType() override {
    assert(reflected_element_type);
    std::ostringstream oss;
    oss << cpp_typename << '<' << reflected_element_type->CppType() << '>';
    return oss.str();
  }
};

struct ReflectedType_Map : ReflectedTypeImpl {
  constexpr static const char* cpp_typename = "std::map";
  ReflectedTypeImpl* reflected_key_type;
  ReflectedTypeImpl* reflected_value_type;

  std::string CppType() override {
    assert(reflected_key_type);
    assert(reflected_value_type);
    std::ostringstream oss;
    oss << cpp_typename << '<' << reflected_key_type->CppType() << ',' << reflected_value_type->CppType()
        << '>';
    return oss.str();
  }
};

struct ReflectedType_Pair : ReflectedTypeImpl {
  constexpr static const char* cpp_typename = "std::pair";
  ReflectedTypeImpl* reflected_first_type;
  ReflectedTypeImpl* reflected_second_type;

  std::string CppType() override {
    assert(reflected_first_type);
    assert(reflected_second_type);
    std::ostringstream oss;
    oss << cpp_typename << '<' << reflected_first_type->CppType() << ',' << reflected_second_type->CppType()
        << '>';
    return oss.str();
  }
};

typedef std::vector<std::pair<ReflectedTypeImpl*, std::string>> StructFieldsVector;

struct ReflectedType_Struct : ReflectedTypeImpl {
  std::string name;
  ReflectedType_Struct* reflected_super = nullptr;
  StructFieldsVector fields;

  std::string CppType() override { return name; }

  std::string CppDeclaration() override {
    std::string result = "struct " + name + (reflected_super ? " : " + reflected_super->name : "") + " {\n";
    for (const auto& f : fields) {
      result += "  " + f.first->CppType() + " " + f.second + ";\n";
    }
    result += "};\n";
    return result;
  }
};

CURRENT_STRUCT(StructSchema) {
  CURRENT_FIELD(type_id, uint64_t, 0u);
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(super_type_id, uint64_t, 0u);
  CURRENT_FIELD(super_name, std::string, "");
  using pair_id_name = std::pair<uint64_t, std::string>;
  CURRENT_FIELD(fields, std::vector<pair_id_name>);

  CURRENT_DEFAULT_CONSTRUCTOR(StructSchema) {}
  CURRENT_CONSTRUCTOR(StructSchema)(const ReflectedTypeImpl* r) {
    const ReflectedType_Struct* reflected_struct = dynamic_cast<const ReflectedType_Struct*>(r);
    if (reflected_struct != nullptr) {
      type_id = static_cast<uint64_t>(reflected_struct->type_id);
      name = reflected_struct->name;
      const ReflectedType_Struct* super = reflected_struct->reflected_super;
      if (super) {
        super_type_id = static_cast<uint64_t>(super->type_id);
        super_name = super->name;
      }
      for (const auto& f : reflected_struct->fields) {
        fields.emplace_back(static_cast<uint64_t>(f.first->type_id), f.second);
      }
    }
  }
};

inline uint64_t rol64(const uint64_t value, const uint64_t nbits) {
  return (value << nbits) | (value >> (-nbits & 63));
}

inline uint64_t rol64(const TypeID type_id, const uint64_t nbits) {
  return rol64(static_cast<uint64_t>(type_id), nbits);
}

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = bricks::CRC32(s.name);
  size_t i = 0u;
  for (const auto& f : s.fields) {
    assert(f.first);
    assert(f.first->type_id != TypeID::INVALID_TYPE);
    hash ^= rol64(f.first->type_id, i + 8) ^ rol64(bricks::CRC32(f.second), i + 16);
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
