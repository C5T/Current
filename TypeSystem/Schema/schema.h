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

#include "../Reflection/reflection.h"
#include "../Serialization/json.h"

#include "../../Bricks/strings/strings.h"
#include "../../Bricks/template/enable_if.h"
#include "../../Bricks/util/singleton.h"

namespace current {
namespace reflection {

// TODO(dkorolev): Refactor `PrimitiveTypesList` to avoid copy-pasting of `operator()(const *_Primitive& p)`.
struct PrimitiveTypesListImpl {
  std::map<TypeID, std::string> cpp_name;
  std::map<TypeID, std::string> fsharp_name;
  PrimitiveTypesListImpl() {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, unused_current_type, fsharp_type) \
  cpp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = #cpp_type;                   \
  fsharp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = fsharp_type;
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  }
};

inline const PrimitiveTypesListImpl& PrimitiveTypesList() {
  return current::Singleton<PrimitiveTypesListImpl>();
}

// Metaprogramming to make it easy to add support for new programming languages to include in the schema.
enum class Language { CPP, FSharp, JSON };

template <Language>
struct LanguageSyntaxImpl;

template <Language L>
struct LanguageSyntax {
  static void Describe(const std::map<TypeID, ReflectedType>& types,
                       const std::vector<TypeID>& order,
                       std::ostream& os,
                       bool headers = true) {
    if (headers) {
      os << LanguageSyntaxImpl<L>::Header();
    }
    typename LanguageSyntaxImpl<L>::FullSchemaPrinter printer(types, os);
    for (TypeID type_id : order) {
      const auto cit = types.find(type_id);
      if (cit == types.end()) {
        os << LanguageSyntaxImpl<L>::ErrorMessageWithTypeId(type_id);
      } else {
        cit->second.Call(printer);
      }
    }
    if (headers) {
      os << LanguageSyntaxImpl<L>::Footer();
    }
  }
};

template <>
struct LanguageSyntaxImpl<Language::CPP> {
  static std::string Header() {
    return "// g++ -c -std=c++11 current.cc\n"
           "\n"
           "#include \"current.h\"\n"
           "\n"
           "namespace current_userspace {\n";
  }

  static std::string Footer() { return "}  // namespace current_userspace\n"; }

  static std::string ErrorMessageWithTypeId(TypeID type_id) {
    return "#error \"Unknown struct with `type_id` = " + current::strings::ToString(type_id) + "\"\n";
  }

  struct FullSchemaPrinter {
    const std::map<TypeID, ReflectedType>& types_;
    std::ostream& os_;

    std::string TypeName(TypeID type_id) const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        return "UNKNOWN_TYPE_" + current::strings::ToString(type_id);
      } else {
        struct CPPTypeNamePrinter {
          const FullSchemaPrinter& self_;
          std::ostringstream& oss_;

          CPPTypeNamePrinter(const FullSchemaPrinter& self, std::ostringstream& oss) : self_(self), oss_(oss) {}

          // `operator()`-s of this block print C++ type name only, without the expansion.
          // They assume the declaration order is respected, and any dependencies have already been listed.
          void operator()(const ReflectedType_Primitive& p) const {
            const auto& globals = PrimitiveTypesList();
            if (globals.cpp_name.count(p.type_id) != 0u) {
              oss_ << globals.cpp_name.at(p.type_id);
            } else {
              oss_ << "UNKNOWN_BASIC_TYPE_" + current::strings::ToString(p.type_id);
            }
          }

          void operator()(const ReflectedType_Enum& e) const { oss_ << e.name; }
          void operator()(const ReflectedType_Vector& v) const {
            oss_ << "std::vector<" << self_.TypeName(v.element_type) << '>';
          }

          void operator()(const ReflectedType_Map& m) const {
            oss_ << "std::map<" << self_.TypeName(m.key_type) << ", " << self_.TypeName(m.value_type) << '>';
          }
          void operator()(const ReflectedType_Pair& p) const {
            oss_ << "std::pair<" << self_.TypeName(p.first_type) << ", " << self_.TypeName(p.second_type)
                 << '>';
          }
          void operator()(const ReflectedType_Optional& o) const {
            oss_ << "Optional<" << self_.TypeName(o.optional_type) << '>';
          }
          void operator()(const ReflectedType_Variant& p) const {
            std::vector<std::string> cases;
            for (TypeID c : p.cases) {
              cases.push_back(self_.TypeName(c));
            }
            oss_ << "Variant<" << current::strings::Join(cases, ", ") << '>';
          }
          void operator()(const ReflectedType_Struct& s) const { oss_ << s.name; }
        };

        std::ostringstream oss;
        cit->second.Call(CPPTypeNamePrinter(*this, oss));
        return oss.str();
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types, std::ostream& os)
        : types_(types), os_(os) {}

    // `operator()`-s of this block print the complete declaration of a type in C++, if necessary.
    // Effectively, they ignore everything but `struct`-s.
    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum&) const {}
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}
    void operator()(const ReflectedType_Variant&) const {}
    void operator()(const ReflectedType_Struct& s) const {
      os_ << "struct " << s.name;
      if (s.super_id != TypeID::CurrentStruct) {
        os_ << " : " << TypeName(s.super_id);
      }
      os_ << " {\n";
      for (const auto& f : s.fields) {
        os_ << "  " << TypeName(f.first) << " " << f.second << ";\n";
      }
      os_ << "};\n";
    }
  };  // struct LanguageSyntax<Language::CPP>::FullSchemaPrinter
};

template <>
struct LanguageSyntaxImpl<Language::FSharp> {
  static std::string Header() {
    return "// fsharpi -r Newtonsoft.Json.dll current.fsx\n"
           "\n"
           "open Newtonsoft.Json\n"
           "let inline JSON o = JsonConvert.SerializeObject(o)\n"
           "let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)\n";
  }

  static std::string Footer() { return ""; }

  static std::string ErrorMessageWithTypeId(TypeID type_id) {
    return "#error \"Unknown struct with `type_id` = " + current::strings::ToString(type_id) + "\"\n";
  }

  struct FullSchemaPrinter {
    const std::map<TypeID, ReflectedType>& types_;
    std::ostream& os_;

    std::string TypeName(TypeID type_id) const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        return "UNKNOWN_TYPE_" + current::strings::ToString(type_id);
      } else {
        struct FSharpTypeNamePrinter {
          const FullSchemaPrinter& self_;
          std::ostringstream& oss_;

          FSharpTypeNamePrinter(const FullSchemaPrinter& self, std::ostringstream& oss)
              : self_(self), oss_(oss) {}

          // `operator()(...)`-s of this block print F# type name only, without the expansion.
          // They assume the declaration order is respected, and any dependencies have already been listed.
          void operator()(const ReflectedType_Primitive& p) const {
            const auto& globals = PrimitiveTypesList();
            if (globals.fsharp_name.count(p.type_id) != 0u) {
              oss_ << globals.fsharp_name.at(p.type_id);
            } else {
              oss_ << "UNKNOWN_BASIC_TYPE_" + current::strings::ToString(p.type_id);
            }
          }
          void operator()(const ReflectedType_Enum& e) const { oss_ << e.name; }
          void operator()(const ReflectedType_Vector& v) const {
            oss_ << self_.TypeName(v.element_type) << " array";
          }
          void operator()(const ReflectedType_Map& m) const {
            oss_ << "System.Collections.Generic.Dictionary<" << self_.TypeName(m.key_type) << ", "
                 << self_.TypeName(m.value_type) << '>';
          }
          void operator()(const ReflectedType_Pair& p) const {
            oss_ << self_.TypeName(p.first_type) << " * " << self_.TypeName(p.second_type);
          }
          void operator()(const ReflectedType_Optional& o) const {
            oss_ << self_.TypeName(o.optional_type) << " option";
          }
          void operator()(const ReflectedType_Variant& p) const {
            std::vector<std::string> cases;
            for (TypeID c : p.cases) {
              cases.push_back(self_.TypeName(c));
            }
            oss_ << "DU_" << current::strings::Join(cases, '_');
          }

          void operator()(const ReflectedType_Struct& s) const { oss_ << s.name; }
        };

        std::ostringstream oss;
        cit->second.Call(FSharpTypeNamePrinter(*this, oss));
        return oss.str();
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types, std::ostream& os)
        : types_(types), os_(os) {}

    // `operator()`-s of this block print complete declarations of F# types.
    // The types that require complete declarations in F# are records and discriminated unions.
    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum&) const {}
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}
    void operator()(const ReflectedType_Variant& p) const {
      std::vector<std::string> cases;
      for (TypeID c : p.cases) {
        cases.push_back(TypeName(c));
      }
      os_ << "\ntype DU_" << current::strings::Join(cases, '_') << " =\n";
      for (const auto& s : cases) {
        os_ << "| " << s << " of " << s << '\n';
      }
    }

    // When dumping a `CURRENT_STRUCT` as an F# record, since inheritance is not supported by Newtonsoft.JSON,
    // all base class variables are hoisted to the top of the record.
    void RecursivelyListStructFields(std::ostringstream& temporary_os, const ReflectedType_Struct& s) const {
      if (s.super_id != TypeID::CurrentStruct) {
        // TODO(dkorolev): Check that `at()` and `Value<>` succeeded.
        RecursivelyListStructFields(temporary_os, Value<ReflectedType_Struct>(types_.at(s.super_id)));
      }
      for (const auto& f : s.fields) {
        temporary_os << "  " << f.second << " : " << TypeName(f.first) << '\n';
      }
    }

    void operator()(const ReflectedType_Struct& s) const {
      std::ostringstream temporary_os;
      RecursivelyListStructFields(temporary_os, s);
      const std::string fields = temporary_os.str();
      if (!fields.empty()) {
        os_ << "\ntype " << s.name << " = {\n" << fields << "}\n";
      } else {
        os_ << "\ntype " << s.name << " = class end\n";
      }
    }

  };  // struct LanguageSyntax<Language::FSharp>::LanguagePrinter
};

template <Language L>
struct LanguageDescribeCaller {
  template <typename T>
  static std::string CallDescribe(const T* instance, bool headers) {
    std::ostringstream oss;
    LanguageSyntax<L>::Describe(instance->types, instance->order, oss, headers);
    return oss.str();
  }
};

// `SchemaInfo` contains information about all the types used in schema, their order
// and dependencies. It can describe the whole schema via `Describe()` method in any supported
// language. `SchemaInfo` acts independently, therefore can be easily saved/loaded using {de}serialization.
CURRENT_STRUCT(SchemaInfo) {
#ifndef _MSC_VER
  // List of all type_id's contained in schema.
  CURRENT_FIELD(types, (std::map<TypeID, ReflectedType>));
#else
  // List of all type_id's contained in schema.
  typedef std::map<TypeID, ReflectedType> t_types_map;
  CURRENT_FIELD(types, t_types_map);
#endif
  // The types in `order` are topologically sorted with respect to all directly and indirectly included types.
  CURRENT_FIELD(order, std::vector<TypeID>);

  CURRENT_DEFAULT_CONSTRUCTOR(SchemaInfo) {}
  CURRENT_CONSTRUCTOR(SchemaInfo)(const SchemaInfo& x) : types(x.types), order(x.order) {}
  CURRENT_CONSTRUCTOR(SchemaInfo)(SchemaInfo && x) : types(std::move(x.types)), order(std::move(x.order)) {}

  template <Language L>
  std::string Describe(bool headers = true) const {
    return LanguageDescribeCaller<L>::CallDescribe(this, headers);
  }
};

template <>
struct LanguageDescribeCaller<Language::JSON> {
  static std::string CallDescribe(const SchemaInfo* instance, bool) { return JSON(*instance); }
};

struct StructSchema {
  struct TypeTraverser {
    TypeTraverser(SchemaInfo& schema) : schema_(schema) {}

    void operator()(const ReflectedType_Struct& s) {
      if (!schema_.types.count(s.type_id)) {
        // Fill `types[type_id]` before traversing everything else to break possible circular dependencies.
        schema_.types.emplace(s.type_id, s);
        if (s.super_id != TypeID::CurrentStruct) {
          Reflector().ReflectedTypeByTypeID(s.super_id).Call(*this);
        }
        for (const auto& f : s.fields) {
          Reflector().ReflectedTypeByTypeID(f.first).Call(*this);
        }
        schema_.order.push_back(s.type_id);
      }
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

    void operator()(const ReflectedType_Variant& p) {
      if (!schema_.types.count(p.type_id)) {
        schema_.types.emplace(p.type_id, p);
        for (TypeID c : p.cases) {
          Reflector().ReflectedTypeByTypeID(c).Call(*this);
        }
        schema_.order.push_back(p.type_id);
      }
    }

   private:
    SchemaInfo& schema_;
  };

  StructSchema() = default;
  StructSchema(const SchemaInfo& schema) : schema_(schema) {}
  StructSchema(SchemaInfo&& schema) : schema_(std::move(schema)) {}

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT(T)> AddType() {}

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT(T)> AddType() {
    Reflector().ReflectType<T>().Call(TypeTraverser(schema_));
  }

  const SchemaInfo& GetSchemaInfo() const { return schema_; }

 private:
  SchemaInfo schema_;
};

}  // namespace reflection
}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_REFLECTION_SCHEMA_H
