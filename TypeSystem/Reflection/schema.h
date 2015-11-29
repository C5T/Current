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

#ifndef CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
#define CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H

#include "reflection.h"

#include "../../Bricks/template/enable_if.h"
#include "../../Bricks/strings/strings.h"

namespace current {
namespace strings {

template <>
struct ToStringImpl<::current::reflection::TypeID> {
  static std::string ToString(::current::reflection::TypeID type_id) {
    return ToStringImpl<uint64_t>::ToString(static_cast<uint64_t>(type_id));
  }
};

}  // namespace strings
}  // namespace current

namespace current {
namespace reflection {

CURRENT_STRUCT(SchemaInfo) {
  CURRENT_FIELD(types, (std::map<TypeID, ReflectedType>));
  // List of all type_id's contained in schema.
  // Ascending index order corresponds to the order required for proper declaring of all the structs.
  CURRENT_FIELD(order, std::vector<TypeID>);
  CURRENT_DEFAULT_CONSTRUCTOR(SchemaInfo) {}
  CURRENT_CONSTRUCTOR(SchemaInfo)(const SchemaInfo& rhs) : order(rhs.order) {
    for (const auto& cit : rhs.types) {
      types.insert(std::make_pair(cit.first, Clone(cit.second)));
    }
  }
  void operator=(const SchemaInfo& rhs) {
    order = rhs.order;
    for (const auto& cit : rhs.types) {
      types.insert(std::make_pair(cit.first, Clone(cit.second)));
    }
  }
};

// Metaprogramming to make it easy to add support for new programming languages to include in the schema.
// TODO(dkorolev): Some of the stuff below could be compile-time, it's just that struct specializations
// are not allowed within other structs, while overloading does its job.
struct Language {
  struct CPP {
    static std::string Header() {
      return "// g++ -c -std=c++11 current.cc\n"
             "\n"
             "#include \"current.h\"\n"
             "\n"
             "namespace current_userspace {\n";
    }
    static std::string Footer() { return "}  // namespace current_userspace\n"; }
    static std::string ErrorMessageWithTypeId(const TypeID type_id) {
      return "#error \"Unknown struct with `type_id` = " + current::strings::ToString(type_id) + "\"\n";
    }
  };
  struct FSharp {
    static std::string Header() {
      return "// fsharpi -r Newtonsoft.Json.dll current.fsx\n"
             "\n"
             "open Newtonsoft.Json\n"
             "let inline JSON o = JsonConvert.SerializeObject(o)\n"
             "let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)\n"
             "\n";
    }
    static std::string Footer() { return ""; }
    static std::string ErrorMessageWithTypeId(const TypeID type_id) {
      // TODO(dkorolev): Probably somewhat different syntax.
      return "#error \"Unknown struct with `type_id` = " + current::strings::ToString(type_id) + "\"\n";
    }
  };
};

struct PrimitiveTypesList {
  std::map<TypeID, std::string> cpp_name;
  std::map<TypeID, std::string> fsharp_name;
  PrimitiveTypesList() {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, unused_current_type, fsharp_type) \
  cpp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = #cpp_type;                   \
  fsharp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = fsharp_type;
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  }
  std::string PrimitiveTypeName(const Language::CPP&, const TypeID type_id) const {
    if (cpp_name.count(type_id) != 0u) {
      return cpp_name.at(type_id);
    }
    return "UNKNOWN_BASIC_TYPE_" + current::strings::ToString(type_id);
  }
  std::string PrimitiveTypeName(const Language::FSharp&, const TypeID type_id) const {
    if (fsharp_name.count(type_id) != 0u) {
      return fsharp_name.at(type_id);
    }
    return "UNKNOWN_BASIC_TYPE_" + current::strings::ToString(type_id);
  }
};

struct StructSchema {
  struct TypeTraverser {
    TypeTraverser(SchemaInfo& schema) : schema_(schema) {}

    void operator()(const ReflectedType_Struct& s) {
      if (schema_.types.count(s.type_id)) {
        return;
      }
      // Fill `types[type_id]` before traversing everything else to break possible circular dependencies.
      schema_.types.emplace(s.type_id, s);
      if (s.super_id != TypeID::CurrentSuper) {
        Reflector().ReflectedTypeByTypeID(s.super_id).Call(*this);
      }
      for (const auto& f : s.fields) {
        Reflector().ReflectedTypeByTypeID(f.first).Call(*this);
      }
      schema_.order.push_back(s.type_id);
    }

    void operator()(const ReflectedType_Primitive& p) {
      if (!schema_.types.count(p.type_id)) {
        schema_.types.emplace(p.type_id, p);
      }
    }

    void operator()(const ReflectedType_Enum& e) {
      if (!schema_.types.count(e.type_id)) {
        schema_.types.emplace(e.type_id, e);
        Reflector().ReflectedTypeByTypeID(e.underlying_type).Call(*this);
        schema_.order.push_back(e.type_id);
      }
    }

    void operator()(const ReflectedType_Vector& v) {
      if (!schema_.types.count(v.type_id)) {
        schema_.types.emplace(v.type_id, v);
        Reflector().ReflectedTypeByTypeID(v.element_type).Call(*this);
        schema_.order.push_back(v.type_id);
      }
    }

    void operator()(const ReflectedType_Map& m) {
      if (!schema_.types.count(m.type_id)) {
        schema_.types.emplace(m.type_id, m);
        Reflector().ReflectedTypeByTypeID(m.key_type).Call(*this);
        Reflector().ReflectedTypeByTypeID(m.value_type).Call(*this);
        schema_.order.push_back(m.type_id);
      }
    }

    void operator()(const ReflectedType_Pair& p) {
      if (!schema_.types.count(p.type_id)) {
        schema_.types.emplace(p.type_id, p);
        Reflector().ReflectedTypeByTypeID(p.first_type).Call(*this);
        Reflector().ReflectedTypeByTypeID(p.second_type).Call(*this);
        schema_.order.push_back(p.type_id);
      }
    }

    void operator()(const ReflectedType_Optional& o) {
      if (!schema_.types.count(o.type_id)) {
        schema_.types.emplace(o.type_id, o);
        Reflector().ReflectedTypeByTypeID(o.optional_type).Call(*this);
        schema_.order.push_back(o.type_id);
      }
    }

    void operator()(const ReflectedType_Polymorphic& p) {
      if (!schema_.types.count(p.type_id)) {
        schema_.types.emplace(p.type_id, p);
        for (const TypeID c : p.cases) {
          Reflector().ReflectedTypeByTypeID(c).Call(*this);
        }
        schema_.order.push_back(p.type_id);
      }
    }

   private:
    SchemaInfo& schema_;
  };

  StructSchema() = default;
  // Temporary disabled => doesn't compile :(
  // StructSchema(const SchemaInfo& schema) : schema_(schema) {}

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT(T)> AddType() {}

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT(T)> AddType() {
    Reflector().ReflectType<T>().Call(TypeTraverser(schema_));
  }

  const SchemaInfo& GetSchemaInfo() const { return schema_; }

  template <typename L>
  std::string Describe(const L& language, bool headers = true) const {
    std::ostringstream oss;
    Describe(language, oss, headers);
    return oss.str();
  }

  template <typename L>
  void Describe(const L& language, std::ostream& os, bool headers = true) const {
    if (headers) {
      os << L::Header();
    }
    for (TypeID type_id : schema_.order) {
      const uint64_t type_prefix = TypePrefix(type_id);
      if (type_prefix == TYPEID_STRUCT_PREFIX) {
        const auto cit = schema_.types.find(type_id);
        if (cit == schema_.types.end()) {
          os << L::ErrorMessageWithTypeId(type_id);
        }
        DescribeStruct(language, Value<ReflectedType_Struct>(cit->second), os);
      }
    }
    if (headers) {
      os << L::Footer();
    }
  }

 private:
  SchemaInfo schema_;
  const PrimitiveTypesList primitive_types_;

  void DescribeStruct(const Language::CPP& language, const ReflectedType_Struct& s, std::ostream& os) const {
    os << "struct " << s.name;
    if (s.super_id != TypeID::CurrentSuper) {
      os << " : " << TypePrintName(language, s.super_id);
    }
    os << " {\n";
    for (const auto& f : s.fields) {
      os << "  " << TypePrintName(language, f.first) << " " << f.second << ";\n";
    }
    os << "};\n";
  }

  void DescribeStruct(const Language::FSharp& language, const ReflectedType_Struct& s, std::ostream& os) const {
    os << "type " << s.name;
    if (s.super_id != TypeID::INVALID_TYPE) {
      os << "  // With unsupported for now base type " << TypePrintName(language, s.super_id);
    }
    os << " = {\n";
    for (const auto& f : s.fields) {
      os << "  " << f.second << " : " << TypePrintName(language, f.first) << '\n';
    }
    os << "}\n";
  }

  template <typename L>
  struct TypePrinter {
    TypePrinter(const L& language, const StructSchema& struct_schema, std::ostringstream& oss)
        : language_(language),
          primitive_types_(struct_schema.primitive_types_),
          schema_info_(struct_schema.schema_),
          struct_schema_(struct_schema),
          oss_(oss) {}

    void operator()(const ReflectedType_Struct& s) { oss_ << s.name; }

    void operator()(const ReflectedType_Primitive& p) {
      oss_ << primitive_types_.PrimitiveTypeName(language_, p.type_id);
    }

    void operator()(const ReflectedType_Enum& e) { oss_ << e.name; }

    void operator()(const ReflectedType_Vector& v) { DescribeVector(language_, v); }

    void operator()(const ReflectedType_Map& m) { DescribeMap(language_, m); }

    void operator()(const ReflectedType_Pair& p) { DescribePair(language_, p); }

    void operator()(const ReflectedType_Optional& o) { DescribeOptional(language_, o); }

    void operator()(const ReflectedType_Polymorphic& p) { DescribePolymorphic(language_, p); }

   private:
    const L& language_;
    const PrimitiveTypesList& primitive_types_;
    const SchemaInfo& schema_info_;
    const StructSchema& struct_schema_;
    std::ostringstream& oss_;

    void DescribeVector(const Language::CPP&, const ReflectedType_Vector& v) {
      oss_ << "std::vector<";
      schema_info_.types.at(v.element_type).Call(*this);
      oss_ << '>';
    }

    void DescribeVector(const Language::FSharp&, const ReflectedType_Vector& v) {
      schema_info_.types.at(v.element_type).Call(*this);
      oss_ << " array";
    }

    void DescribePair(const Language::CPP&, const ReflectedType_Pair& p) {
      oss_ << "std::pair<";
      schema_info_.types.at(p.first_type).Call(*this);
      oss_ << ", ";
      schema_info_.types.at(p.second_type).Call(*this);
      oss_ << '>';
    }
    void DescribePair(const Language::FSharp&, const ReflectedType_Pair& p) {
      schema_info_.types.at(p.first_type).Call(*this);
      oss_ << " * ";
      schema_info_.types.at(p.second_type).Call(*this);
    }

    void DescribeMap(const Language::CPP&, const ReflectedType_Map& m) {
      oss_ << "std::map<";
      schema_info_.types.at(m.key_type).Call(*this);
      oss_ << ", ";
      schema_info_.types.at(m.value_type).Call(*this);
      oss_ << '>';
    }
    void DescribeMap(const Language::FSharp&, const ReflectedType_Map& m) {
      // TODO(dkorolev): `System.Collections.Generic.Dictionary<>`? See how well does it play with `fsharpi`.
      oss_ << "UNSUPPORTED_MAP<";
      schema_info_.types.at(m.key_type).Call(*this);
      oss_ << ", ";
      schema_info_.types.at(m.value_type).Call(*this);
      oss_ << '>';
    }

    void DescribeOptional(const Language::CPP&, const ReflectedType_Optional& o) {
      oss_ << "Optional<";
      schema_info_.types.at(o.optional_type).Call(*this);
      oss_ << '>';
    }
    void DescribeOptional(const Language::FSharp&, const ReflectedType_Optional& o) {
      schema_info_.types.at(o.optional_type).Call(*this);
      oss_ << " option";
    }

    void DescribePolymorphic(const Language::CPP& language, const ReflectedType_Polymorphic& p) {
      std::vector<std::string> names;
      for (const TypeID c : p.cases) {
        names.push_back(struct_schema_.TypePrintName(language, c));
      }
      const std::string class_name = std::string(p.required ? "" : "Optional") + "Polymorphic<";
      oss_ << class_name << current::strings::Join(names, ", ") << '>';
    }
    void DescribePolymorphic(const Language::FSharp& language, const ReflectedType_Polymorphic& p) const {
      std::vector<std::string> names;
      for (const TypeID c : p.cases) {
        names.push_back(struct_schema_.TypePrintName(language, c));
      }
      const std::string class_name = std::string(p.required ? "" : "Optional") + "Polymorphic<";
      oss_ << class_name << current::strings::Join(names, ", ") << '>';
    }
  };

  template <typename L>
  std::string TypePrintName(const L& language, const TypeID type_id) const {
    const uint64_t type_prefix = TypePrefix(type_id);
    if (type_prefix == TYPEID_BASIC_PREFIX) {
      return primitive_types_.PrimitiveTypeName(language, type_id);
    }
    const auto cit = schema_.types.find(type_id);
    if (cit == schema_.types.end()) {
      return "UNKNOWN_TYPE_" + current::strings::ToString(type_id);
    } else {
      std::ostringstream oss;
      TypePrinter<L> type_printer(language, *this, oss);
      cit->second.Call(type_printer);
      return oss.str();
    }
  }
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
