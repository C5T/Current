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

#include "../../Bricks/strings/util.h"
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
constexpr uint64_t TYPEID_STRUCT_PREFIX   = 920u;
constexpr uint64_t TYPEID_OPTIONAL_PREFIX = 921u;
constexpr uint64_t TYPEID_VARIANT_PREFIX  = 922u;
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
constexpr uint64_t TYPEID_STRUCT_TYPE   = TYPEID_TYPE_RANGE * TYPEID_STRUCT_PREFIX;
constexpr uint64_t TYPEID_OPTIONAL_TYPE = TYPEID_TYPE_RANGE * TYPEID_OPTIONAL_PREFIX;
constexpr uint64_t TYPEID_VARIANT_TYPE  = TYPEID_TYPE_RANGE * TYPEID_VARIANT_PREFIX;
// Base TypeID-s for STL containers.
constexpr uint64_t TYPEID_VECTOR_TYPE = TYPEID_TYPE_RANGE * TYPEID_VECTOR_PREFIX;
constexpr uint64_t TYPEID_SET_TYPE    = TYPEID_TYPE_RANGE * TYPEID_SET_PREFIX;
constexpr uint64_t TYPEID_PAIR_TYPE   = TYPEID_TYPE_RANGE * TYPEID_PAIR_PREFIX;
constexpr uint64_t TYPEID_MAP_TYPE    = TYPEID_TYPE_RANGE * TYPEID_MAP_PREFIX;
// clang-format on

// clang-format off
CURRENT_ENUM(TypeID, uint64_t){
  NotYetReadyButYouGuysHangInThere = 0u,
  CurrentStruct = 1u,
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, current_type, fs_type, md_type) \
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
  CURRENT_FIELD(type_id, TypeID);
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(description, Optional<std::string>);
  CURRENT_DEFAULT_CONSTRUCTOR(ReflectedType_Struct_Field){};
  CURRENT_CONSTRUCTOR(ReflectedType_Struct_Field)(
      TypeID type_id, const std::string& name, const Optional<std::string> description)
      : type_id(type_id), name(name), description(description) {}
};

CURRENT_STRUCT(ReflectedType_Struct, ReflectedTypeBase) {
  CURRENT_FIELD(native_name, std::string);
  CURRENT_FIELD(super_id, TypeID, TypeID::UninitializedType);
  CURRENT_FIELD(template_id, Optional<TypeID>);  // For instantiated `CURRENT_STRUCT_T`-s.
  CURRENT_FIELD(fields, std::vector<ReflectedType_Struct_Field>);
  CURRENT_DEFAULT_CONSTRUCTOR(ReflectedType_Struct) {}
  // The `CanonicalName()` method serves two purposes:
  // 1) it generates the unique name, for a template-instantiated `CURRENT_STRUCT_T` as well, and
  // 2) it makes sure the the 'autogen -> #include -> autogen' loop does not change TypeIDs of templated types.
  std::string CanonicalName() const {
    if (Exists(template_id)) {
      // TODO(dkorolev): This code relies on `<>` at the end of the names of `CURRENT_STRUCT_T`-s. Fix it.
      CURRENT_ASSERT(native_name.length() > 2);
      CURRENT_ASSERT(native_name.substr(native_name.length() - 2) == "_Z");
      return native_name.substr(0, native_name.length() - 2) + "_T" + current::ToString(Value(template_id));
    } else {
      return native_name;
    }
  }
};

using ReflectedType = Variant<ReflectedType_Primitive,
                              ReflectedType_Enum,
                              ReflectedType_Vector,
                              ReflectedType_Map,
                              ReflectedType_Pair,
                              ReflectedType_Optional,
                              ReflectedType_Variant,
                              ReflectedType_Struct>;

inline uint64_t ROL64(const TypeID type_id, size_t nbits) {
  return current::ROL64(static_cast<uint64_t>(type_id), nbits);
}

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = current::CRC32(s.CanonicalName());
  size_t i = 0u;
  hash ^=
      ROL64(static_cast<TypeID>(static_cast<uint64_t>(s.super_id) ^ static_cast<uint64_t>(TypeID::CurrentStruct)), 7u);
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
