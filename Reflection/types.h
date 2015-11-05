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

#ifndef CURRENT_REFLECTION_TYPES_H
#define CURRENT_REFLECTION_TYPES_H

#include <cassert>
#include <string>
#include <sstream>
#include <vector>
#include <memory>

#include "base.h"

#include "../Bricks/util/crc32.h"

namespace current {
namespace reflection {

constexpr uint64_t TYPEID_TYPE_RANGE = static_cast<uint64_t>(1e16);
constexpr uint64_t TYPEID_BASIC_TYPE = 900u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_COLLECTION_TYPE = 901u * TYPEID_TYPE_RANGE;
constexpr uint64_t TYPEID_STRUCT_TYPE = 902u * TYPEID_TYPE_RANGE;

enum class TypeID : uint64_t {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, unused_cpp_type, current_type) \
  current_type = TYPEID_BASIC_TYPE + typeid_index,
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
};

struct ReflectedTypeImpl {
  TypeID type_id;
  virtual std::string CppType() = 0;
  virtual std::string CppDeclaration() { return ""; }
  virtual ~ReflectedTypeImpl() = default;
};

#define CURRENT_DECLARE_PRIMITIVE_TYPE(unused_typeid_index, cpp_type, current_type) \
  struct ReflectedType_##current_type : ReflectedTypeImpl {                         \
    TypeID type_id = TypeID::current_type;                                          \
    std::string CppType() override { return #cpp_type; }                            \
  };
#include "primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE

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
  std::string super_name;
  StructFieldsVector fields;

  std::string CppType() override { return name; }

  std::string CppDeclaration() override {
    std::string result = "struct " + name + super_name + " {\n";
    for (const auto& f : fields) {
      result += "  " + f.first->CppType() + " " + f.second + ";\n";
    }
    result += "};\n";
    return result;
  }
};

inline TypeID CalculateTypeID(const ReflectedType_Struct& s) {
  uint64_t hash = bricks::CRC32(s.name);
  size_t i = 0u;
  for (const auto& f : s.fields) {
    hash ^= (static_cast<uint64_t>(bricks::CRC32(f.second)) << 8) ^
            (static_cast<uint64_t>(bricks::CRC32(f.first->CppType())) << (16 + (i++ % 17)));
  }
  return static_cast<TypeID>(TYPEID_STRUCT_TYPE + hash % TYPEID_TYPE_RANGE);
}

inline TypeID CalculateTypeID(const ReflectedType_Vector& v) {
  return static_cast<TypeID>(TYPEID_COLLECTION_TYPE + bricks::CRC32(v.reflected_element_type->CppType()));
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

#endif  // CURRENT_REFLECTION_TYPES_H
