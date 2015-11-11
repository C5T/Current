/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H

#include <unordered_set>

#include "types.h"
#include "reflection.h"

namespace current {
namespace reflection {

CURRENT_STRUCT(TypeSchema) {
  CURRENT_FIELD(type_id, uint64_t, 0u);
  CURRENT_FIELD(included_types, std::vector<uint64_t>);
};

CURRENT_STRUCT(StructSchema) {
  CURRENT_FIELD(type_id, uint64_t, 0u);
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(super_type_id, uint64_t, 0u);
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
      }
      for (const auto& f : reflected_struct->fields) {
        fields.emplace_back(static_cast<uint64_t>(f.first->type_id), f.second);
      }
    }
  }
};

CURRENT_STRUCT(FullTypeSchema) {
  using map_typeid_structschema = std::map<uint64_t, StructSchema>;
  CURRENT_FIELD(struct_schemas, map_typeid_structschema);
  using map_typeid_typeschema = std::map<uint64_t, TypeSchema>;
  CURRENT_FIELD(type_schemas, map_typeid_typeschema);
  CURRENT_FIELD(ordered_struct_list, std::vector<uint64_t>);

  template <typename T>
  typename std::enable_if<std::is_base_of<CurrentSuper, T>::value>::type AddStruct() {
    TraverseType(Reflector().ReflectType<T>());
  }

  std::string CppDescription(const uint64_t type_id_) {
    const TypeID type_id = static_cast<TypeID>(type_id_);
    if (IsTypeIDInRange(type_id, TYPEID_STRUCT_TYPE)) {
      if (struct_schemas.count(type_id_) == 0u) {
        return "UNKNOWN_STRUCT";
      }
      std::ostringstream oss;
      const StructSchema& struct_schema = struct_schemas[type_id_];
      oss << "struct " << struct_schema.name;
      if (struct_schema.super_type_id) {
        oss << " : " << CppType(struct_schema.super_type_id);
      }
      oss << " {\n";
      for (const auto& f : struct_schema.fields) {
        oss << "  " << CppType(f.first) << " " << f.second << ";\n";
      }
      oss << "};\n";
      return oss.str();
    } else {
      return CppType(type_id_);
    }
  }

 private:
  void TraverseType(const ReflectedTypeImpl* reflected_type) {
    assert(reflected_type);
    const uint64_t type_id = static_cast<uint64_t>(reflected_type->type_id);
    if (IsTypeIDInRange(reflected_type->type_id, TYPEID_STRUCT_TYPE) && struct_schemas.count(type_id) == 0u) {
      const ReflectedType_Struct* s = dynamic_cast<const ReflectedType_Struct*>(reflected_type);
      if (s->reflected_super) {
        TraverseType(s->reflected_super);
      }
      for (const auto& f : s->fields) {
        TraverseType(f.first);
      }
      struct_schemas[type_id] = StructSchema(reflected_type);
      ordered_struct_list.push_back(type_id);
    }

    if (type_schemas.count(type_id) == 0u) {
      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_VECTOR_TYPE)) {
        ReflectedTypeImpl* reflected_element_type =
            dynamic_cast<const ReflectedType_Vector*>(reflected_type)->reflected_element_type;
        assert(reflected_element_type);
        TypeSchema type_schema;
        type_schema.type_id = type_id;
        type_schema.included_types.push_back(static_cast<uint64_t>(reflected_element_type->type_id));
        type_schemas[type_id] = type_schema;
        TraverseType(reflected_element_type);
      }

      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_PAIR_TYPE)) {
        ReflectedTypeImpl* reflected_first_type =
            dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_first_type;
        ReflectedTypeImpl* reflected_second_type =
            dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_second_type;
        assert(reflected_first_type);
        assert(reflected_second_type);
        TypeSchema type_schema;
        type_schema.type_id = type_id;
        type_schema.included_types.push_back(static_cast<uint64_t>(reflected_first_type->type_id));
        type_schema.included_types.push_back(static_cast<uint64_t>(reflected_second_type->type_id));
        type_schemas[type_id] = type_schema;
        TraverseType(reflected_first_type);
        TraverseType(reflected_second_type);
      }

      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_MAP_TYPE)) {
        ReflectedTypeImpl* reflected_key_type =
            dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_key_type;
        ReflectedTypeImpl* reflected_value_type =
            dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_value_type;
        assert(reflected_key_type);
        assert(reflected_value_type);
        TypeSchema type_schema;
        type_schema.type_id = type_id;
        type_schema.included_types.push_back(static_cast<uint64_t>(reflected_key_type->type_id));
        type_schema.included_types.push_back(static_cast<uint64_t>(reflected_value_type->type_id));
        type_schemas[type_id] = type_schema;
        TraverseType(reflected_key_type);
        TraverseType(reflected_value_type);
      }

      // Primitive type.
    }
  }

  std::string CppType(const uint64_t type_id_) {
    const TypeID type_id = static_cast<TypeID>(type_id_);
    if (IsTypeIDInRange(type_id, TYPEID_STRUCT_TYPE)) {
      if (struct_schemas.count(type_id_) == 0u) {
        return "UNKNOWN_STRUCT";
      }
      return struct_schemas[type_id_].name;
    } else {
      if (IsTypeIDInRange(type_id, TYPEID_BASIC_TYPE)) {
        if (type_id == TypeID::Double) {
          return "double";
        }
        // TODO: basic type names extraction.
        return "basic_type";
      }
      if (type_schemas.count(type_id_) == 0u) {
        return "UNKNOWN";
      }
      if (IsTypeIDInRange(type_id, TYPEID_VECTOR_TYPE)) {
        return DescribeCppVector(type_schemas[type_id_]);
      }
      return "UNKNOWN";
    }
  }

  std::string DescribeCppVector(const TypeSchema& schema) {
    assert(schema.included_types.size() == 1u);
    return "std::vector<" + CppType(schema.included_types[0]) + ">";
  }
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
