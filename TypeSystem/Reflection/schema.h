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

#include "reflection.h"

#include "../../Bricks/strings/strings.h"

namespace current {
namespace reflection {

CURRENT_STRUCT(TypeInfo) {
  CURRENT_FIELD(type_id, uint64_t, 0u);
  CURRENT_FIELD(included_types, std::vector<uint64_t>);
};

CURRENT_STRUCT(StructInfo) {
  CURRENT_FIELD(type_id, uint64_t, 0u);
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(super_type_id, uint64_t, 0u);
  using pair_id_name = std::pair<uint64_t, std::string>;
  CURRENT_FIELD(fields, std::vector<pair_id_name>);

  CURRENT_DEFAULT_CONSTRUCTOR(StructInfo) {}
  CURRENT_CONSTRUCTOR(StructInfo)(const ReflectedTypeImpl* r) {
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

CURRENT_STRUCT(SchemaInfo) {
  using map_typeid_structinfo = std::map<uint64_t, StructInfo>;
  CURRENT_FIELD(structs, map_typeid_structinfo);
  using map_typeid_typeinfo = std::map<uint64_t, TypeInfo>;
  CURRENT_FIELD(types, map_typeid_typeinfo);
  CURRENT_FIELD(ordered_struct_list, std::vector<uint64_t>);
};

struct PrimitiveTypesList {
  std::map<uint64_t, std::string> cpp_name;
  PrimitiveTypesList() {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, unused_current_type) \
  cpp_name[TYPEID_BASIC_TYPE + typeid_index] = #cpp_type;
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  }
};

struct StructSchema {
  StructSchema() = default;
  StructSchema(const SchemaInfo& schema) : schema_(schema) {}

  template <typename T>
  typename std::enable_if<std::is_base_of<CurrentSuper, T>::value>::type AddStruct() {
    TraverseType(Reflector().ReflectType<T>());
  }

  const SchemaInfo& GetSchemaInfo() const { return schema_; }

  std::string CppDescription(const uint64_t type_id) {
    const uint64_t type_prefix = TypePrefix(type_id);
    if (type_prefix == TYPEID_STRUCT_PREFIX) {
      if (schema_.structs.count(type_id) == 0u) {
        return "#error \"Unknown struct with `type_id` = " + bricks::strings::ToString(type_id) + "\"\n";
      }
      std::ostringstream oss;
      const StructInfo& struct_info = schema_.structs[type_id];
      oss << "struct " << struct_info.name;
      if (struct_info.super_type_id) {
        oss << " : " << CppType(struct_info.super_type_id);
      }
      oss << " {\n";
      for (const auto& f : struct_info.fields) {
        oss << "  " << CppType(f.first) << " " << f.second << ";\n";
      }
      oss << "};\n";
      return oss.str();
    } else {
      return CppType(type_id);
    }
  }

 private:
  SchemaInfo schema_;
  const PrimitiveTypesList primitive_types_;

  void TraverseType(const ReflectedTypeImpl* reflected_type) {
    assert(reflected_type);
    const uint64_t type_id = static_cast<uint64_t>(reflected_type->type_id);
    const uint64_t type_prefix = TypePrefix(type_id);
    if (type_prefix == TYPEID_STRUCT_PREFIX && schema_.structs.count(type_id) == 0u) {
      const ReflectedType_Struct* s = dynamic_cast<const ReflectedType_Struct*>(reflected_type);
      if (s->reflected_super) {
        TraverseType(s->reflected_super);
      }
      for (const auto& f : s->fields) {
        TraverseType(f.first);
      }
      schema_.structs[type_id] = StructInfo(reflected_type);
      schema_.ordered_struct_list.push_back(type_id);
    }

    if (schema_.types.count(type_id) == 0u) {
      if (type_prefix == TYPEID_VECTOR_PREFIX) {
        ReflectedTypeImpl* reflected_element_type =
            dynamic_cast<const ReflectedType_Vector*>(reflected_type)->reflected_element_type;
        assert(reflected_element_type);
        TypeInfo type_info;
        type_info.type_id = type_id;
        type_info.included_types.push_back(static_cast<uint64_t>(reflected_element_type->type_id));
        schema_.types[type_id] = type_info;
        TraverseType(reflected_element_type);
      }

      if (type_prefix == TYPEID_PAIR_PREFIX) {
        ReflectedTypeImpl* reflected_first_type =
            dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_first_type;
        ReflectedTypeImpl* reflected_second_type =
            dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_second_type;
        assert(reflected_first_type);
        assert(reflected_second_type);
        TypeInfo type_info;
        type_info.type_id = type_id;
        type_info.included_types.push_back(static_cast<uint64_t>(reflected_first_type->type_id));
        type_info.included_types.push_back(static_cast<uint64_t>(reflected_second_type->type_id));
        schema_.types[type_id] = type_info;
        TraverseType(reflected_first_type);
        TraverseType(reflected_second_type);
      }

      if (type_prefix == TYPEID_MAP_PREFIX) {
        ReflectedTypeImpl* reflected_key_type =
            dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_key_type;
        ReflectedTypeImpl* reflected_value_type =
            dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_value_type;
        assert(reflected_key_type);
        assert(reflected_value_type);
        TypeInfo type_info;
        type_info.type_id = type_id;
        type_info.included_types.push_back(static_cast<uint64_t>(reflected_key_type->type_id));
        type_info.included_types.push_back(static_cast<uint64_t>(reflected_value_type->type_id));
        schema_.types[type_id] = type_info;
        TraverseType(reflected_key_type);
        TraverseType(reflected_value_type);
      }

      // Primitive type.
    }
  }

  std::string CppType(const uint64_t type_id) {
    const uint64_t type_prefix = TypePrefix(type_id);
    if (type_prefix == TYPEID_STRUCT_PREFIX) {
      if (schema_.structs.count(type_id) == 0u) {
        return "UNKNOWN_STRUCT_" + bricks::strings::ToString(type_id);
      }
      return schema_.structs[type_id].name;
    } else {
      if (type_prefix == TYPEID_BASIC_PREFIX) {
        if (primitive_types_.cpp_name.count(type_id) != 0u) {
          return primitive_types_.cpp_name.at(type_id);
        }
        return "UNKNOWN_BASIC_TYPE_" + bricks::strings::ToString(type_id);
      }
      if (schema_.types.count(type_id) == 0u) {
        return "UNKNOWN_INTERMEDIATE_TYPE_" + bricks::strings::ToString(type_id);
      }
      if (type_prefix == TYPEID_VECTOR_PREFIX) {
        return DescribeCppVector(schema_.types[type_id]);
      }
      if (type_prefix == TYPEID_PAIR_PREFIX) {
        return DescribeCppPair(schema_.types[type_id]);
      }
      if (type_prefix == TYPEID_MAP_PREFIX) {
        return DescribeCppMap(schema_.types[type_id]);
      }
      return "UNHANDLED_TYPE_" + bricks::strings::ToString(type_id);
    }
  }

  std::string DescribeCppVector(const TypeInfo& type_info) {
    assert(type_info.included_types.size() == 1u);
    return "std::vector<" + CppType(type_info.included_types[0]) + '>';
  }

  std::string DescribeCppPair(const TypeInfo& type_info) {
    assert(type_info.included_types.size() == 2u);
    return "std::pair<" + CppType(type_info.included_types[0]) + ", " + CppType(type_info.included_types[1]) +
           '>';
  }

  std::string DescribeCppMap(const TypeInfo& type_info) {
    assert(type_info.included_types.size() == 2u);
    return "std::map<" + CppType(type_info.included_types[0]) + ", " + CppType(type_info.included_types[1]) +
           '>';
  }
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
