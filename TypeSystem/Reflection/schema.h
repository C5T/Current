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

#include "../../Bricks/template/enable_if.h"
#include "../../Bricks/strings/strings.h"

namespace bricks {
namespace strings {

template <>
struct ToStringImpl<::current::reflection::TypeID> {
  static std::string ToString(::current::reflection::TypeID type_id) {
    return ToStringImpl<uint64_t>::ToString(static_cast<uint64_t>(type_id));
  }
};

}  // namespace strings
}  // namespace bricks

namespace current {
namespace reflection {

CURRENT_STRUCT(TypeInfo) {
  CURRENT_FIELD(type_id, TypeID, TypeID::INVALID_TYPE);
  // Ascending index order in vector corresponds to left-to-right order in the definition of the type,
  // i.e. for `map<int32_t, string>`:
  //   included_types[0] = TypeID::Int32
  //   included_types[1] = TypeID::String
  CURRENT_FIELD(included_types, std::vector<TypeID>);
};

CURRENT_STRUCT(StructInfo) {
  CURRENT_FIELD(type_id, TypeID, TypeID::INVALID_TYPE);
  CURRENT_FIELD(name, std::string, "");
  CURRENT_FIELD(super_type_id, TypeID, TypeID::INVALID_TYPE);
  CURRENT_FIELD(fields, (std::vector<std::pair<TypeID, std::string>>));

  CURRENT_DEFAULT_CONSTRUCTOR(StructInfo) {}
  CURRENT_CONSTRUCTOR(StructInfo)(const std::shared_ptr<ReflectedTypeImpl> r) {
    const ReflectedType_Struct* reflected_struct = dynamic_cast<const ReflectedType_Struct*>(r.get());
    if (reflected_struct != nullptr) {
      type_id = reflected_struct->type_id;
      name = reflected_struct->name;
      const ReflectedType_Struct* super =
          dynamic_cast<const ReflectedType_Struct*>(reflected_struct->reflected_super.get());
      if (super != nullptr) {
        super_type_id = super->type_id;
      }
      for (const auto& f : reflected_struct->fields) {
        fields.emplace_back(f.first->type_id, f.second);
      }
    }
  }
};

CURRENT_STRUCT(SchemaInfo) {
  CURRENT_FIELD(structs, (std::map<TypeID, StructInfo>));
  CURRENT_FIELD(types, (std::map<TypeID, TypeInfo>));
  // List of the struct type_id's contained in schema.
  // Ascending index order corresponds to the order required for proper declaring of all the structs.
  CURRENT_FIELD(ordered_struct_list, std::vector<TypeID>);
};

struct PrimitiveTypesList {
  std::map<TypeID, std::string> cpp_name;
  PrimitiveTypesList() {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, unused_current_type) \
  cpp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = #cpp_type;
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  }
};

struct StructSchema {
  StructSchema() = default;
  StructSchema(const SchemaInfo& schema) : schema_(schema) {}

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT(T)> AddType() {}

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT(T)> AddType() {
    TraverseType(Reflector().ReflectType<T>());
  }

  const SchemaInfo& GetSchemaInfo() const { return schema_; }

  std::string CppDescription(const TypeID type_id, bool with_dependencies = false) {
    const uint64_t type_prefix = TypePrefix(type_id);
    if (type_prefix == TYPEID_STRUCT_PREFIX) {
      if (schema_.structs.count(type_id) == 0u) {
        return "#error \"Unknown struct with `type_id` = " + bricks::strings::ToString(type_id) + "\"\n";
      }
      const StructInfo& struct_info = schema_.structs[type_id];
      std::ostringstream oss;

      if (with_dependencies) {
        const std::vector<TypeID> structs_to_describe = ListStructDependencies(type_id);
        for (const TypeID id : structs_to_describe) {
          oss << CppDescription(id) << "\n";
        }
      }

      oss << "struct " << struct_info.name;
      if (struct_info.super_type_id != TypeID::INVALID_TYPE) {
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

  void TraverseType(const std::shared_ptr<ReflectedTypeImpl> reflected_type) {
    assert(reflected_type);

    const TypeID type_id = reflected_type->type_id;
    const uint64_t type_prefix = TypePrefix(type_id);

    // Do not process primitive or already known complex type or struct.
    if (type_prefix == TYPEID_BASIC_PREFIX || schema_.structs.count(type_id) || schema_.types.count(type_id)) {
      return;
    }

    if (type_prefix == TYPEID_STRUCT_PREFIX) {
      const ReflectedType_Struct* s = dynamic_cast<const ReflectedType_Struct*>(reflected_type.get());
      if (s->reflected_super) {
        TraverseType(s->reflected_super);
      }
      for (const auto& f : s->fields) {
        TraverseType(f.first);
      }
      schema_.structs[type_id] = StructInfo(reflected_type);
      schema_.ordered_struct_list.push_back(type_id);
      return;
    }

    if (type_prefix == TYPEID_VECTOR_PREFIX) {
      const std::shared_ptr<ReflectedTypeImpl> reflected_element_type =
          std::dynamic_pointer_cast<ReflectedType_Vector>(reflected_type)->reflected_element_type;
      assert(reflected_element_type);
      TypeInfo type_info;
      type_info.type_id = type_id;
      type_info.included_types.push_back(reflected_element_type->type_id);
      schema_.types[type_id] = type_info;
      TraverseType(reflected_element_type);
      return;
    }

    if (type_prefix == TYPEID_PAIR_PREFIX) {
      const std::shared_ptr<ReflectedTypeImpl> reflected_first_type =
          std::dynamic_pointer_cast<ReflectedType_Pair>(reflected_type)->reflected_first_type;
      const std::shared_ptr<ReflectedTypeImpl> reflected_second_type =
          std::dynamic_pointer_cast<ReflectedType_Pair>(reflected_type)->reflected_second_type;
      assert(reflected_first_type);
      assert(reflected_second_type);
      TypeInfo type_info;
      type_info.type_id = type_id;
      type_info.included_types.push_back(reflected_first_type->type_id);
      type_info.included_types.push_back(reflected_second_type->type_id);
      schema_.types[type_id] = type_info;
      TraverseType(reflected_first_type);
      TraverseType(reflected_second_type);
      return;
    }

    if (type_prefix == TYPEID_MAP_PREFIX) {
      std::shared_ptr<ReflectedTypeImpl> reflected_key_type =
          std::dynamic_pointer_cast<ReflectedType_Map>(reflected_type)->reflected_key_type;
      std::shared_ptr<ReflectedTypeImpl> reflected_value_type =
          std::dynamic_pointer_cast<ReflectedType_Map>(reflected_type)->reflected_value_type;
      assert(reflected_key_type);
      assert(reflected_value_type);
      TypeInfo type_info;
      type_info.type_id = type_id;
      type_info.included_types.push_back(reflected_key_type->type_id);
      type_info.included_types.push_back(reflected_value_type->type_id);
      schema_.types[type_id] = type_info;
      TraverseType(reflected_key_type);
      TraverseType(reflected_value_type);
      return;
    }
  }

  std::vector<TypeID> ListStructDependencies(const TypeID struct_type_id) {
    std::vector<TypeID> result;
    const uint64_t struct_type_prefix = TypePrefix(struct_type_id);
    assert(struct_type_prefix == TYPEID_STRUCT_PREFIX);
    RecursiveListStructDependencies(struct_type_id, result, false);
    return result;
  }

  void RecursiveListStructDependencies(const TypeID type_id,
                                       std::vector<TypeID>& dependency_list,
                                       bool should_be_listed = true) {
    const uint64_t type_prefix = TypePrefix(type_id);
    // Skip primitive types.
    if (type_prefix == TYPEID_BASIC_PREFIX) {
      return;
    }

    // Process structs.
    if (type_prefix == TYPEID_STRUCT_PREFIX) {
      assert(schema_.structs.count(type_id));
      if (std::find(dependency_list.begin(), dependency_list.end(), type_id) != dependency_list.end()) {
        return;
      }
      const StructInfo& struct_info = schema_.structs[type_id];
      if (struct_info.super_type_id != TypeID::INVALID_TYPE) {
        RecursiveListStructDependencies(struct_info.super_type_id, dependency_list);
      }
      for (const auto& cit : struct_info.fields) {
        RecursiveListStructDependencies(cit.first, dependency_list);
      }
      if (should_be_listed) {
        dependency_list.push_back(type_id);
      }
    } else {
      // Otherwise, this is a complex type, which must exist in `schema_.types`.
      assert(schema_.types.count(type_id));
      const TypeInfo& type_info = schema_.types[type_id];
      for (const TypeID included_type : type_info.included_types) {
        RecursiveListStructDependencies(included_type, dependency_list);
      }
    }
  }

  std::string CppType(const TypeID type_id) {
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
