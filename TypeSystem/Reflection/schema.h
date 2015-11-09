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

struct FullTypeSchema {
  std::vector<StructSchema> struct_schemas;

  template <typename T>
  typename std::enable_if<std::is_base_of<CurrentSuper, T>::value>::type AddStruct() {
    TraverseType(Reflector().ReflectType<T>());
  }

 private:
  std::unordered_set<TypeID, TypeIDHash> known_types_;

  void TraverseType(const ReflectedTypeImpl* reflected_type) {
    if (known_types_.count(reflected_type->type_id) == 0u) {
      assert(reflected_type);
      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_STRUCT_TYPE)) {
        const ReflectedType_Struct* s = dynamic_cast<const ReflectedType_Struct*>(reflected_type);
        if (s->reflected_super) {
          TraverseType(s->reflected_super);
        }
        for (const auto& f : s->fields) {
          TraverseType(f.first);
        }
        struct_schemas.push_back(StructSchema(reflected_type));
      }

      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_VECTOR_TYPE)) {
        TraverseType(dynamic_cast<const ReflectedType_Vector*>(reflected_type)->reflected_element_type);
      }

      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_PAIR_TYPE)) {
        TraverseType(dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_first_type);
        TraverseType(dynamic_cast<const ReflectedType_Pair*>(reflected_type)->reflected_second_type);
      }

      if (IsTypeIDInRange(reflected_type->type_id, TYPEID_MAP_TYPE)) {
        TraverseType(dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_key_type);
        TraverseType(dynamic_cast<const ReflectedType_Map*>(reflected_type)->reflected_value_type);
      }

      known_types_.insert(reflected_type->type_id);
    }
  }
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
