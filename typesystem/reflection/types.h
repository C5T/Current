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

#include <string>
#include <sstream>
#include <vector>
#include <map>
#include <memory>

#include "../base.h"
#include "../enum.h"
#include "../struct.h"
#include "../optional.h"
#include "../variant.h"

#include "../../bricks/strings/util.h"
#include "../../bricks/template/enable_if.h"
#include "../../bricks/util/crc32.h"
#include "../../bricks/util/rol.h"

namespace current {
namespace reflection {

// clang-format off
// Special type prefix for structs containing a reference to itself.
// Basic types prefixes.
constexpr uint64_t TYPEID_BASIC_PREFIX = 900u;
constexpr uint64_t TYPEID_ENUM_PREFIX  = 901u;
// Current complex types prefixes.
constexpr uint64_t TYPEID_STRUCT_PREFIX   = 920u;
constexpr uint64_t TYPEID_OPTIONAL_PREFIX = 921u;
constexpr uint64_t TYPEID_VARIANT_PREFIX  = 922u;
// STL containers prefixes.
constexpr uint64_t TYPEID_VECTOR_PREFIX         = 931u;
constexpr uint64_t TYPEID_SET_PREFIX            = 932u;
constexpr uint64_t TYPEID_PAIR_PREFIX           = 933u;
constexpr uint64_t TYPEID_MAP_PREFIX            = 934u;
constexpr uint64_t TYPEID_UNORDERED_SET_PREFIX  = 935u;
constexpr uint64_t TYPEID_UNORDERED_MAP_PREFIX  = 936u;
// Cyclic dependencies resolution logic.
constexpr uint64_t TYPEID_CYCLIC_DEPENDENCY_PREFIX  = 999u;

// Range of possible TypeID-s for each prefix.
constexpr uint64_t TYPEID_TYPE_RANGE = static_cast<uint64_t>(1e16);
// Base TypeID for structs containing a reference to itself.
// Base TypeID-s for basic types.
constexpr uint64_t TYPEID_BASIC_TYPE = TYPEID_TYPE_RANGE * TYPEID_BASIC_PREFIX;
constexpr uint64_t TYPEID_ENUM_TYPE  = TYPEID_TYPE_RANGE * TYPEID_ENUM_PREFIX;
// Base TypeID-s for Current complex types.
constexpr uint64_t TYPEID_STRUCT_TYPE   = TYPEID_TYPE_RANGE * TYPEID_STRUCT_PREFIX;
constexpr uint64_t TYPEID_OPTIONAL_TYPE = TYPEID_TYPE_RANGE * TYPEID_OPTIONAL_PREFIX;
constexpr uint64_t TYPEID_VARIANT_TYPE  = TYPEID_TYPE_RANGE * TYPEID_VARIANT_PREFIX;
// Base TypeID-s for STL containers.
constexpr uint64_t TYPEID_VECTOR_TYPE        = TYPEID_TYPE_RANGE * TYPEID_VECTOR_PREFIX;
constexpr uint64_t TYPEID_SET_TYPE           = TYPEID_TYPE_RANGE * TYPEID_SET_PREFIX;
constexpr uint64_t TYPEID_PAIR_TYPE          = TYPEID_TYPE_RANGE * TYPEID_PAIR_PREFIX;
constexpr uint64_t TYPEID_MAP_TYPE           = TYPEID_TYPE_RANGE * TYPEID_MAP_PREFIX;
constexpr uint64_t TYPEID_UNORDERED_SET_TYPE = TYPEID_TYPE_RANGE * TYPEID_UNORDERED_SET_PREFIX;
constexpr uint64_t TYPEID_UNORDERED_MAP_TYPE = TYPEID_TYPE_RANGE * TYPEID_UNORDERED_MAP_PREFIX;
// Cyclic dependencies resolution logic.
constexpr uint64_t TYPEID_CYCLIC_DEPENDENCY_TYPE = TYPEID_TYPE_RANGE * TYPEID_CYCLIC_DEPENDENCY_PREFIX;
// clang-format on

// clang-format off
CURRENT_ENUM(TypeID, uint64_t) {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type, typescript_type) \
  current_type = TYPEID_BASIC_TYPE + typeid_index,
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  UninitializedType = static_cast<uint64_t>(-1ll)
};
// clang-format on

inline uint64_t TypePrefix(const uint64_t type_id) { return type_id / TYPEID_TYPE_RANGE; }

inline uint64_t TypePrefix(const TypeID type_id) { return TypePrefix(static_cast<uint64_t>(type_id)); }

CURRENT_STRUCT(ReflectedTypeBase) { CURRENT_FIELD(type_id, TypeID, TypeID::UninitializedType); };

// clang-format off
CURRENT_STRUCT(ReflectedType_Primitive, ReflectedTypeBase) {
  // Default constructor required for using in `Variant`, here and in the structs below.
  CURRENT_CONSTRUCTOR(ReflectedType_Primitive)(TypeID id = TypeID::UninitializedType) {
    ReflectedTypeBase::type_id = id;
  }
};
// clang-format on

CURRENT_STRUCT(ReflectedType_Enum, ReflectedTypeBase) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(underlying_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Enum)(const std::string& name = "", TypeID rt = TypeID::UninitializedType)
      : name(name), underlying_type(rt) {
    ReflectedTypeBase::type_id = static_cast<TypeID>(TYPEID_ENUM_TYPE + current::CRC32(name));
  }
};

CURRENT_STRUCT(ReflectedType_Vector, ReflectedTypeBase) {
  CURRENT_FIELD(element_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Vector)(TypeID re = TypeID::UninitializedType) : element_type(re) {}
};

CURRENT_STRUCT(ReflectedType_Map, ReflectedTypeBase) {
  CURRENT_FIELD(key_type, TypeID);
  CURRENT_FIELD(value_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Map)(TypeID rk = TypeID::UninitializedType, TypeID rv = TypeID::UninitializedType)
      : key_type(rk), value_type(rv) {}
};

CURRENT_STRUCT(ReflectedType_UnorderedMap, ReflectedTypeBase) {
  CURRENT_FIELD(key_type, TypeID);
  CURRENT_FIELD(value_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_UnorderedMap)(TypeID rk = TypeID::UninitializedType,
                                                  TypeID rv = TypeID::UninitializedType)
      : key_type(rk), value_type(rv) {}
};

CURRENT_STRUCT(ReflectedType_Set, ReflectedTypeBase) {
  CURRENT_FIELD(value_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Set)(TypeID rv = TypeID::UninitializedType) : value_type(rv) {}
};

CURRENT_STRUCT(ReflectedType_UnorderedSet, ReflectedTypeBase) {
  CURRENT_FIELD(value_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_UnorderedSet)(TypeID rv = TypeID::UninitializedType) : value_type(rv) {}
};

CURRENT_STRUCT(ReflectedType_Pair, ReflectedTypeBase) {
  CURRENT_FIELD(first_type, TypeID);
  CURRENT_FIELD(second_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Pair)(TypeID rf = TypeID::UninitializedType, TypeID rs = TypeID::UninitializedType)
      : first_type(rf), second_type(rs) {}
};

CURRENT_STRUCT(ReflectedType_Optional, ReflectedTypeBase) {
  CURRENT_FIELD(optional_type, TypeID);
  CURRENT_CONSTRUCTOR(ReflectedType_Optional)(TypeID ro = TypeID::UninitializedType) : optional_type(ro) {}
};

CURRENT_STRUCT(ReflectedType_Variant, ReflectedTypeBase) {
  CURRENT_FIELD(name, std::string);  // `"Variant_B_" + Join(cases, '_') + "_E"` or the user-supplied name.
  CURRENT_FIELD(cases, std::vector<TypeID>);
  CURRENT_DEFAULT_CONSTRUCTOR(ReflectedType_Variant) {}
};

CURRENT_STRUCT(ReflectedType_Struct_Field) {
  CURRENT_FIELD(type_id, TypeID, TypeID::UninitializedType);
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(description, Optional<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(ReflectedType_Struct_Field){};
  CURRENT_CONSTRUCTOR(ReflectedType_Struct_Field)(
      TypeID type_id, const std::string& name, const Optional<std::string> description)
      : type_id(type_id), name(name), description(description) {}
};

CURRENT_STRUCT(ReflectedType_Struct, ReflectedTypeBase) {
  CURRENT_FIELD(native_name, std::string);
  CURRENT_FIELD(super_id, Optional<TypeID>);
  CURRENT_FIELD(super_name, Optional<std::string>);
  CURRENT_FIELD(template_inner_id, Optional<TypeID>);  // For instantiated `CURRENT_STRUCT_T`-s.
  CURRENT_FIELD(template_inner_name, Optional<std::string>);
  CURRENT_FIELD(fields, std::vector<ReflectedType_Struct_Field>);
  CURRENT_DEFAULT_CONSTRUCTOR(ReflectedType_Struct) {}
  // The `TemplateInnerTypeExpandedName()` method serves two purposes:
  // 1) It generates the unique name for the structure for the schema-exported `CURRENT_STRUCT_T`-s.
  //    (As `CURRENT_STRUCT_T`-s are exported as regular `CURRENT_STRUCT`-s, one per instantiation.)
  // 2) It preserves the TypeID-s the way they used to be before the refactoring of the reflector.
  std::string TemplateInnerTypeExpandedName() const {
    if (Exists(template_inner_id)) {
      CURRENT_ASSERT(native_name.length() > 2);
      CURRENT_ASSERT(native_name.substr(native_name.length() - 2) == "_Z");
      return native_name.substr(0, native_name.length() - 2) + "_T" + current::ToString(Value(template_inner_id));
    } else {
      return native_name;
    }
  }
  // TemplateFullNameDecorator() returns a function that converts names ending with "_Z"
  // into the names ending with "_T...", where `...` is the TypeID of the `template_inner_id` of this struct.
  // Prerequisite: The struct is an instantiation of the template (or is declared so).
  std::function<std::string(const std::string&)> TemplateFullNameDecorator() const {
    CURRENT_ASSERT(Exists(template_inner_id));
    const TypeID t = Value(template_inner_id);
    return [t](const std::string& input) -> std::string {
      CURRENT_ASSERT(input.length() >= 3);
      CURRENT_ASSERT(input.substr(input.length() - 2) == "_Z");
      return input.substr(0, input.length() - 1) + 'T' + current::ToString(static_cast<uint64_t>(t));
    };
  }
};

using ReflectedType = Variant<ReflectedType_Primitive,
                              ReflectedType_Enum,
                              ReflectedType_Vector,
                              ReflectedType_Map,
                              ReflectedType_UnorderedMap,
                              ReflectedType_Set,
                              ReflectedType_UnorderedSet,
                              ReflectedType_Pair,
                              ReflectedType_Optional,
                              ReflectedType_Variant,
                              ReflectedType_Struct>;

inline uint64_t ROL64(const TypeID type_id, size_t nbits) {
  return current::ROL64(static_cast<uint64_t>(type_id), nbits);
}

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = current::CRC32(s.TemplateInnerTypeExpandedName());
  // The `1` is here for historical reasons, and should be kept. -- D.K.
  const uint64_t base = Exists(s.super_id) ? static_cast<uint64_t>(Value(s.super_id)) : 1;
  hash ^= ROL64(static_cast<TypeID>(base ^ 1), 7u);

  size_t i = 0u;
  for (const auto& f : s.fields) {
    CURRENT_ASSERT(f.type_id != TypeID::UninitializedType);
    hash ^= ROL64(f.type_id, i + 17u) ^ current::ROL64(current::CRC32(f.name), i + 29u);
    ++i;
  }
  return static_cast<TypeID>(TYPEID_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Vector& v) {
  CURRENT_ASSERT(v.element_type != TypeID::UninitializedType);
  return static_cast<TypeID>(TYPEID_VECTOR_TYPE + ROL64(v.element_type, 3u) % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Pair& p) {
  CURRENT_ASSERT(p.first_type != TypeID::UninitializedType);
  CURRENT_ASSERT(p.second_type != TypeID::UninitializedType);
  uint64_t hash = ROL64(p.first_type, 5u) ^ ROL64(p.second_type, 11u);
  return static_cast<TypeID>(TYPEID_PAIR_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Map& m) {
  CURRENT_ASSERT(m.key_type != TypeID::UninitializedType);
  CURRENT_ASSERT(m.value_type != TypeID::UninitializedType);
  uint64_t hash = ROL64(m.key_type, 5u) ^ ROL64(m.value_type, 11u);
  return static_cast<TypeID>(TYPEID_MAP_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_UnorderedMap& m) {
  CURRENT_ASSERT(m.key_type != TypeID::UninitializedType);
  CURRENT_ASSERT(m.value_type != TypeID::UninitializedType);
  uint64_t hash = ROL64(m.key_type, 5u) ^ ROL64(m.value_type, 11u);
  return static_cast<TypeID>(TYPEID_UNORDERED_MAP_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Set& s) {
  CURRENT_ASSERT(s.value_type != TypeID::UninitializedType);
  uint64_t hash = ROL64(s.value_type, 5u) ^ ROL64(s.value_type, 11u);
  return static_cast<TypeID>(TYPEID_SET_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_UnorderedSet& s) {
  CURRENT_ASSERT(s.value_type != TypeID::UninitializedType);
  uint64_t hash = ROL64(s.value_type, 5u) ^ ROL64(s.value_type, 11u);
  return static_cast<TypeID>(TYPEID_UNORDERED_SET_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Optional& o) {
  CURRENT_ASSERT(o.optional_type != TypeID::UninitializedType);
  return static_cast<TypeID>(TYPEID_OPTIONAL_TYPE + ROL64(o.optional_type, 5u) % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Variant& v) {
  uint64_t hash = current::CRC32(v.name);
  size_t i = 0u;
  for (const auto& c : v.cases) {
    hash ^= ROL64(c, i * 3u + 17u);
    ++i;
  }
  return static_cast<TypeID>(TYPEID_VARIANT_TYPE + hash % TYPEID_TYPE_RANGE);
}

// Enable `CalculateTypeID` for bare and smart pointers.
// TODO(dk+mz): remove?
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
