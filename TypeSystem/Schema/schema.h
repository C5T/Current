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

#ifndef CURRENT_TYPE_SYSTEM_SCHEMA_SCHEMA_H
#define CURRENT_TYPE_SYSTEM_SCHEMA_SCHEMA_H

#include "../../port.h"

#include <unordered_map>
#include <unordered_set>

#include "json_schema_format.h"

#include "../Reflection/reflection.h"
#include "../Serialization/json.h"

#include "../../Bricks/strings/strings.h"
#include "../../Bricks/util/singleton.h"
#include "../../Bricks/util/sha256.h"

namespace current {
namespace reflection {

// The container to optionally pass to schema generator to have certain types exposed under certain names.
CURRENT_STRUCT(NamespaceToExpose) {
  CURRENT_FIELD(name, std::string);
  CURRENT_FIELD(types, (std::map<std::string, TypeID>));
  CURRENT_CONSTRUCTOR(NamespaceToExpose)(const std::string& name = "Schema") : name(name) {}
  template <typename T>
  void AddType(const std::string& exported_type_name) {
    types[exported_type_name] = Value<ReflectedTypeBase>(Reflector().ReflectType<T>()).type_id;
  }
  template <typename T>
  void AddType() {
    types[CurrentTypeName<T>()] = Value<ReflectedTypeBase>(Reflector().ReflectType<T>()).type_id;
  }
};

// TODO(dkorolev): Refactor `PrimitiveTypesList` to avoid copy-pasting of `operator()(const *_Primitive& p)`.
struct PrimitiveTypesListImpl final {
  std::map<TypeID, std::string> cpp_name;
  std::map<TypeID, std::string> fsharp_name;
  std::map<TypeID, std::string> markdown_name;
  PrimitiveTypesListImpl() {
#define CURRENT_DECLARE_PRIMITIVE_TYPE(typeid_index, cpp_type, unused_current_type, fs_type, md_type) \
  cpp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = #cpp_type;                        \
  fsharp_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = fs_type;                       \
  markdown_name[static_cast<TypeID>(TYPEID_BASIC_TYPE + typeid_index)] = md_type;
#include "../primitive_types.dsl.h"
#undef CURRENT_DECLARE_PRIMITIVE_TYPE
  }
};

inline const PrimitiveTypesListImpl& PrimitiveTypesList() {
  return current::Singleton<PrimitiveTypesListImpl>();
}

inline void AppendAsMultilineCommentIndentedTwoSpaces(std::ostream& os, const std::string& description) {
  for (const auto& line : strings::Split(description, '\n')) {
    os << "  // " << line << '\n';
  }
}

// Metaprogramming to make it easy to add support for new programming languages to include in the schema.
enum class Language : int {
  begin = 0,
  InternalFormat = 0,  // Schema as the JSON of an internal format, which can be parsed back.
  Current,             // C++, `CURRENT_STRUCT`-s.
  CPP,                 // C++, native `struct`-s.
  FSharp,              // F#.
  Markdown,            // [GitHub] Markdown.
  JSON,                // A compact JSON we use to describe schema to third parties.
  end
};

template <Language begin, Language end>
struct ForEachLanguageImpl final {
  template <typename F>
  static void Call(F&& f) {
    f.template PerLanguage<begin>();
    ForEachLanguageImpl<static_cast<Language>(static_cast<int>(begin) + 1), end>::Call(std::forward<F>(f));
  }
};

template <Language empty_range>
struct ForEachLanguageImpl<empty_range, empty_range> final {
  template <typename F>
  static void Call(F&&) {}
};

template <typename F>
void ForEachLanguage(F&& f) {
  ForEachLanguageImpl<Language::begin, Language::end>::Call(std::forward<F>(f));
}

inline Language& operator++(Language& language) {
  language = static_cast<Language>(static_cast<int>(language) + 1);
  return language;
}

template <Language>
struct LanguageSyntaxImpl;

template <Language L>
struct LanguageSyntax final {
  static void Describe(const std::map<TypeID, ReflectedType>& types,
                       const std::vector<TypeID>& order,
                       std::ostream& os,
                       bool headers,
                       const Optional<NamespaceToExpose>& namespace_to_expose) {
    const std::string unique_hash = SHA256(JSON(order)).substr(0, 16);
    if (headers) {
      os << LanguageSyntaxImpl<L>::Header(unique_hash);
    }
    {
      // Wrap `FullSchemaPrinter` into its own scope, so that its destructor would execture prior to `Footer()`.
      typename LanguageSyntaxImpl<L>::FullSchemaPrinter printer(types, os, unique_hash, namespace_to_expose);
      for (TypeID type_id : order) {
        const auto cit = types.find(type_id);
        if (cit == types.end()) {
          os << LanguageSyntaxImpl<L>::ErrorMessageWithTypeId(type_id, printer);  // LCOV_EXCL_LINE
        } else {
          cit->second.Call(printer);
        }
      }
    }
    if (headers) {
      os << LanguageSyntaxImpl<L>::Footer(unique_hash);
    }
  }
};

enum class CPPLanguageSelector { CurrentStructs, NativeStructs };

template <CPPLanguageSelector>
struct CurrentStructPrinter;

template <>
struct CurrentStructPrinter<CPPLanguageSelector::CurrentStructs> {
  static void PrintCurrentStruct(std::ostream& os,
                                 const ReflectedType_Struct& s,
                                 std::function<std::string(TypeID)> type_name) {
    const std::string struct_name = s.CanonicalName();
    os << "CURRENT_STRUCT(" << struct_name;
    if (s.super_id != TypeID::CurrentStruct) {
      os << ", " << type_name(s.super_id);
    }
    os << ") {\n";
    for (const auto& f : s.fields) {
      // Type name should be put into parentheses if it contains commas. Putting all type names
      // into parentheses won't hurt, I've added the condition purely for aesthetic purposes. -- D.K.
      const std::string raw_type_name = type_name(f.type_id);
      const std::string type_name =
          raw_type_name.find(',') == std::string::npos ? raw_type_name : '(' + raw_type_name + ')';
      os << "  CURRENT_FIELD(" << f.name << ", " << type_name << ");\n";
      if (Exists(f.description)) {
        os << "  CURRENT_FIELD_DESCRIPTION(" << f.name << ", \""
           << strings::EscapeForCPlusPlus(Value(f.description)) << "\");\n";
      }
    }
    os << "};\n";
    os << "using T" << current::ToString(s.type_id) << " = " << struct_name << ";\n\n";
  }
  static void PrintCurrentEnum(std::ostream& os,
                               const ReflectedType_Enum& e,
                               std::function<std::string(TypeID)> type_name) {
    os << "CURRENT_ENUM(" << e.name << ", " << type_name(e.underlying_type) << ") {};\n";
    os << "using T" << current::ToString(e.type_id) << " = " << e.name << ";\n\n";
  }
  static void PrintCurrentVariant(std::ostream& os,
                                  const ReflectedType_Variant& v,
                                  std::function<std::string(TypeID)> type_name) {
    std::vector<std::string> cases;
    for (TypeID c : v.cases) {
      cases.push_back(type_name(c));
    }
    os << "CURRENT_VARIANT(" << v.name << ", " << current::strings::Join(cases, ", ") << ");\n";
    os << "using T" << current::ToString(v.type_id) << " = " << v.name << ";\n\n";
  }
};

template <>
struct CurrentStructPrinter<CPPLanguageSelector::NativeStructs> {
  static void PrintCurrentStruct(std::ostream& os,
                                 const ReflectedType_Struct& s,
                                 std::function<std::string(TypeID)> type_name) {
    os << "struct " << s.CanonicalName();
    if (s.super_id != TypeID::CurrentStruct) {
      os << " : " << type_name(s.super_id);
    }
    os << " {\n";
    bool first_field = true;
    for (const auto& f : s.fields) {
      if (Exists(f.description)) {
        if (!first_field) {
          os << '\n';
        }
        AppendAsMultilineCommentIndentedTwoSpaces(os, Value(f.description));
      }
      os << "  " << type_name(f.type_id) << " " << f.name << ";\n";
      first_field = false;
    }
    os << "};\n";
  }
  static void PrintCurrentEnum(std::ostream& os,
                               const ReflectedType_Enum& e,
                               std::function<std::string(TypeID)> type_name) {
    os << "enum class " << e.name << " : " << type_name(e.underlying_type) << " {};\n";
  }
  static void PrintCurrentVariant(std::ostream& os,
                                  const ReflectedType_Variant& v,
                                  std::function<std::string(TypeID)> type_name) {
    std::vector<std::string> cases;
    for (TypeID c : v.cases) {
      cases.push_back(type_name(c));
    }
    os << "using " << v.name << " = Variant<" << current::strings::Join(cases, ", ") << ">;\n";
  }
};

template <CPPLanguageSelector CPP_LANGUAGE_SELECTOR>
struct LanguageSyntaxCPP : CurrentStructPrinter<CPP_LANGUAGE_SELECTOR> {
  static std::string Header(const std::string& unique_hash) {
    return "// The `current.h` file is the one from `https://github.com/C5T/Current`.\n"
           "// Compile with `-std=c++11` or higher.\n"
           "\n" +
           ("#ifndef CURRENT_USERSPACE_" + current::strings::ToUpper(unique_hash) + "\n") +
           ("#define CURRENT_USERSPACE_" + current::strings::ToUpper(unique_hash) + "\n") +
           "\n"
           "#include \"current.h\"\n"
           "\n"
           "// clang-format off\n"
           "\n";
  }

  static std::string Footer(const std::string& unique_hash) {
    return "\n"
           "// clang-format on\n"
           "\n" +
           ("#endif  // CURRENT_USERSPACE_" + current::strings::ToUpper(unique_hash) + "\n");
  }

  struct FullSchemaPrinter final {
    const std::map<TypeID, ReflectedType>& types_;
    std::ostream& os_;
    const std::string unique_hash_;
    const Optional<NamespaceToExpose>& namespace_to_expose_;

    std::string TypeName(TypeID type_id, const std::string& nmspc = "") const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        return "UNKNOWN_TYPE_" + current::ToString(type_id);  // LCOV_EXCL_LINE
      } else {
        struct CurrentTypeNamePrinter final {
          const FullSchemaPrinter& self_;
          std::ostringstream& oss_;
          const std::string& nmspc_;

          CurrentTypeNamePrinter(const FullSchemaPrinter& self,
                                 std::ostringstream& oss,
                                 const std::string& nmspc)
              : self_(self), oss_(oss), nmspc_(nmspc) {}

          // `operator()`-s of this block print C++ type name only, without the expansion.
          // They assume the declaration order is respected, and any dependencies have already been listed.
          void operator()(const ReflectedType_Primitive& p) const {
            const auto& globals = PrimitiveTypesList();
            if (globals.cpp_name.count(p.type_id) != 0u) {
              oss_ << globals.cpp_name.at(p.type_id);
            } else {
              oss_ << "UNKNOWN_BASIC_TYPE_" + current::ToString(p.type_id);  // LCOV_EXCL_LINE
            }
          }

          void operator()(const ReflectedType_Enum& e) const {
            if (!nmspc_.empty()) {
              oss_ << nmspc_ << "::";
            }
            oss_ << e.name;
          }
          void operator()(const ReflectedType_Vector& v) const {
            oss_ << "std::vector<" << self_.TypeName(v.element_type, nmspc_) << '>';
          }

          void operator()(const ReflectedType_Map& m) const {
            oss_ << "std::map<" << self_.TypeName(m.key_type, nmspc_) << ", "
                 << self_.TypeName(m.value_type, nmspc_) << '>';
          }
          void operator()(const ReflectedType_Pair& p) const {
            oss_ << "std::pair<" << self_.TypeName(p.first_type, nmspc_) << ", "
                 << self_.TypeName(p.second_type, nmspc_) << '>';
          }
          void operator()(const ReflectedType_Optional& o) const {
            oss_ << "Optional<" << self_.TypeName(o.optional_type, nmspc_) << '>';
          }
          void operator()(const ReflectedType_Variant& v) const {
            if (!nmspc_.empty()) {
              oss_ << nmspc_ << "::";
            }
            oss_ << v.name;
          }
          void operator()(const ReflectedType_Struct& s) const {
            if (!nmspc_.empty()) {
              oss_ << nmspc_ << "::";
            }
            oss_ << s.CanonicalName();
          }
        };

        std::ostringstream oss;
        cit->second.Call(CurrentTypeNamePrinter(*this, oss, nmspc));
        return oss.str();
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types,
                      std::ostream& os,
                      const std::string& unique_hash,
                      const Optional<NamespaceToExpose>& namespace_to_expose)
        : types_(types), os_(os), unique_hash_(unique_hash), namespace_to_expose_(namespace_to_expose) {
      os_ << "namespace current_userspace_" << unique_hash_ << " {\n";
      if (CPP_LANGUAGE_SELECTOR == CPPLanguageSelector::CurrentStructs) {
        os_ << '\n';
      }
    }
    ~FullSchemaPrinter() {
      const std::string nmspc = "USERSPACE_" + current::strings::ToUpper(unique_hash_);

      os_ << "}  // namespace current_userspace_" << unique_hash_ << '\n';

      if (CPP_LANGUAGE_SELECTOR == CPPLanguageSelector::CurrentStructs) {
        os_ << '\n';
        os_ << "CURRENT_NAMESPACE(" << nmspc << ") {\n";
        for (const auto& input_type : types_) {
          const auto& type_substance = input_type.second;
          if (Exists<ReflectedType_Struct>(type_substance) || Exists<ReflectedType_Variant>(type_substance) ||
              Exists<ReflectedType_Enum>(type_substance)) {
            const auto type_name = TypeName(input_type.first);
            os_ << "  CURRENT_NAMESPACE_TYPE(" << type_name << ", current_userspace_" << unique_hash_
                << "::" << type_name << ");\n";
          }
        }
        os_ << "};  // CURRENT_NAMESPACE(" << nmspc << ")\n";

        os_ << '\n' << "namespace current {\n"
            << "namespace type_evolution {\n" << '\n';

        // To only output distinct `Variant<>` & `CURRENT_VARIANT`-s once.
        std::unordered_set<uint64_t> variant_evolutor_already_output;

        for (const auto& input_type : types_) {
          const auto type_name = TypeName(input_type.first);
          const auto namespaced_type_name = TypeName(input_type.first, nmspc);
          const auto& type_substance = input_type.second;
          if (Exists<ReflectedType_Struct>(type_substance)) {
            // Default evolutor for `CURRENT_STRUCT`.
            const ReflectedType_Struct& s = Value<ReflectedType_Struct>(type_substance);
            std::vector<std::string> fields;
            for (const auto& f : s.fields) {
              fields.push_back(f.name);
            }
            os_ << "// Default evolution for struct `" << type_name << "`.\n";
            os_ << "template <typename NAMESPACE, typename EVOLUTOR>\n"
                << "struct Evolve<NAMESPACE, " << namespaced_type_name << ", EVOLUTOR> {\n"
                << "  template <typename INTO,\n"
                << "            class CHECK = NAMESPACE,\n"
                << "            class = std::enable_if_t<::current::is_same_or_base_of<" << nmspc
                << ", CHECK>::value>>\n"
                << "  static void Go(const typename NAMESPACE::" << type_name << "& from,\n"
                << "                 typename INTO::" << type_name << "& into) {\n"
                << "      static_assert(::current::reflection::FieldCounter<typename INTO::" << type_name
                << ">::value == " << fields.size() << ",\n"
                << "                    \"Custom evolutor required.\");\n";
            if (s.super_id != TypeID::CurrentStruct) {
              const std::string super_name = TypeName(s.super_id);
              os_ << "      Evolve<NAMESPACE, " << nmspc << "::" << super_name << ", EVOLUTOR>::"
                  << "template Go<INTO>(static_cast<const typename NAMESPACE::" << super_name
                  << "&>(from), static_cast<typename INTO::" << super_name << "&>(into));\n";
            }
            for (const auto& f : fields) {
              os_ << "      Evolve<NAMESPACE, decltype(from." << f << "), EVOLUTOR>::"
                  << "template Go<INTO>(from." << f << ", into." << f << ");\n";
            }
            if (fields.empty()) {
              os_ << "      static_cast<void>(from);\n"
                  << "      static_cast<void>(into);\n";
            }
            os_ << "  }\n"
                << "};\n" << '\n';
          } else if (Exists<ReflectedType_Variant>(type_substance)) {
            // Default evolutor for `CURRENT_VARIANT`, or for a plain `Variant<>`.
            std::vector<std::string> cases;
            std::vector<std::string> fully_specified_cases;
            std::vector<TypeID> types_list;
            uint64_t variant_inner_type_list_hash = 0ull;
            uint64_t multiplier = 1u;
            for (TypeID c : Value<ReflectedType_Variant>(type_substance).cases) {
              types_list.push_back(c);
              const auto& case_name = TypeName(c);
              cases.push_back(case_name);
              fully_specified_cases.push_back(nmspc + "::" + case_name);
              variant_inner_type_list_hash += static_cast<uint64_t>(c) * multiplier;
              multiplier = multiplier * 17 + 1;
            }
            if (variant_evolutor_already_output.count(variant_inner_type_list_hash)) {
              continue;
            }
            variant_evolutor_already_output.insert(variant_inner_type_list_hash);
            os_ << "// Default evolution for `Variant<" << current::strings::Join(cases, ", ") << ">`.\n";
            const std::string evltr = nmspc + '_' + type_name + "_Cases";
            os_ << "template <typename DST, typename FROM_NAMESPACE, typename INTO, typename EVOLUTOR>\n"
                << "struct " << evltr << " {\n"
                << "  DST& into;\n"
                << "  explicit " << evltr << "(DST& into) : into(into) {}\n";
            for (const auto& c : cases) {
              os_ << "  void operator()(const typename FROM_NAMESPACE::" << c << "& value) const {\n"
                  << "    using into_t = typename INTO::" << c << ";\n"
                  << "    into = into_t();\n"
                  << "    Evolve<FROM_NAMESPACE, typename FROM_NAMESPACE::" << c << ", EVOLUTOR>"
                  << "::template Go<INTO>(value, Value<into_t>(into));\n"
                  << "  }\n";
            }
            const std::string vrnt = "::current::VariantImpl<VARIANT_NAME_HELPER, TypeListImpl<" +
                                     current::strings::Join(fully_specified_cases, ", ") + ">>";
            os_ << "};\n"
                << "template <typename NAMESPACE, typename EVOLUTOR, typename VARIANT_NAME_HELPER>\n"
                << "struct Evolve<NAMESPACE, " << vrnt << ", EVOLUTOR> {\n"
                << "  // TODO(dkorolev): A `static_assert` to ensure the number of cases is the same.\n"
                << "  template <typename INTO,\n"
                << "            typename CUSTOM_INTO_VARIANT_TYPE,\n"
                << "            class CHECK = NAMESPACE,\n"
                << "            class = std::enable_if_t<::current::is_same_or_base_of<" << nmspc
                << ", CHECK>::value>>\n"
                << "  static void Go(const " << vrnt << "& from,\n"
                << "                 CUSTOM_INTO_VARIANT_TYPE& into) {\n"
                << "    from.Call(" << evltr << "<decltype(into), NAMESPACE, INTO, EVOLUTOR>(into));\n"
                << "  }\n"
                << "};\n" << '\n';
          } else if (Exists<ReflectedType_Enum>(type_substance)) {
            // Default evolutor for `CURRENT_ENUM`.
            const auto& e = Value<ReflectedType_Enum>(type_substance);
            os_ << "// Default evolution for `CURRENT_ENUM(" << e.name << ")`.\n"
                << "template <typename NAMESPACE, typename EVOLUTOR>\n"
                << "struct Evolve<NAMESPACE, " << nmspc << "::" << e.name << ", EVOLUTOR> {\n"
                << "  template <typename INTO>\n"
                << "  static void Go(" << nmspc << "::" << e.name << " from,\n"
                << "                 typename INTO::" << e.name << "& into) {\n"
                << "    // TODO(dkorolev): Check enum underlying type, but not too strictly to be extensible.\n"
                << "    into = static_cast<typename INTO::" << e.name << ">(from);\n"
                << "  }\n"
                << "};\n" << '\n';
          } else if (Exists<ReflectedType_Optional>(type_substance)) {
            // Default evolutor for this particular `Optional<T>`.
            // The global, top-level one, can only work if the underlying type does not change.
            const auto& o = Value<ReflectedType_Optional>(type_substance);
            const std::string tn = TypeName(o.optional_type);
            const auto namespaced_type_name = TypeName(o.optional_type, nmspc);
            if (TypePrefix(o.optional_type) != TYPEID_BASIC_PREFIX) {
              // No need to spell out evolution of `Optional<>` for basic types, string-s, and millis/micros.
              os_ << "// Default evolution for `Optional<" << tn << ">`.\n"
                  << "template <typename NAMESPACE, typename EVOLUTOR>\n"
                  << "struct Evolve<NAMESPACE, Optional<" << namespaced_type_name << ">, EVOLUTOR> {\n"
                  << "  template <typename INTO>\n"
                  << "  static void Go(const Optional<" << namespaced_type_name << ">& from, "
                  << "Optional<typename INTO::" << tn << ">& into) {\n"
                  << "    if (Exists(from)) {\n"
                  << "      typename INTO::" << tn << " evolved;\n"
                  << "      Evolve<NAMESPACE, typename " << namespaced_type_name << ", EVOLUTOR>"
                  << "::template Go<INTO>(Value(from), evolved);\n"
                  << "      into = evolved;\n"
                  << "    } else {\n"
                  << "      into = nullptr;\n"
                  << "    }\n"
                  << "  }\n"
                  << "};\n" << '\n';
            }
          }
        }
        os_ << "}  // namespace current::type_evolution\n"
            << "}  // namespace current\n";

        if (Exists(namespace_to_expose_)) {
          const NamespaceToExpose& expose = Value(namespace_to_expose_);
          os_ << "\n"
              << "// Privileged types.\n"
              << "CURRENT_DERIVED_NAMESPACE(" << expose.name << ", " << nmspc << ") {\n";
          for (const auto& t : expose.types) {
            os_ << "  CURRENT_NAMESPACE_TYPE(" << t.first << ", current_userspace_" << unique_hash_
                << "::" << TypeName(t.second) << ");\n";
          }
          os_ << "};  // CURRENT_NAMESPACE(" << expose.name << ")\n";
        }
      }
    }

    // `operator()`-s of this block print the complete declaration of a type in C++, if necessary.
    // Effectively, they ignore everything but `struct`-s and `Variant`-s.
    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum& e) const {
      CurrentStructPrinter<CPP_LANGUAGE_SELECTOR>::PrintCurrentEnum(
          os_, e, [this](TypeID id) -> std::string { return TypeName(id); });
    }
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}
    void operator()(const ReflectedType_Variant& v) const {
      CurrentStructPrinter<CPP_LANGUAGE_SELECTOR>::PrintCurrentVariant(
          os_, v, [this](TypeID id) -> std::string { return TypeName(id); });
    }
    void operator()(const ReflectedType_Struct& s) const {
      CurrentStructPrinter<CPP_LANGUAGE_SELECTOR>::PrintCurrentStruct(
          os_, s, [this](TypeID id) -> std::string { return TypeName(id); });
    }
  };  // struct LanguageSyntaxCPP::FullSchemaPrinter

  // LCOV_EXCL_START
  static std::string ErrorMessageWithTypeId(TypeID type_id, FullSchemaPrinter&) {
    return "#error \"Unknown struct with `type_id` = " + current::ToString(type_id) + "\"\n";
  }
  // LCOV_EXCL_STOP
};

template <>
struct LanguageSyntaxImpl<Language::Current> final : LanguageSyntaxCPP<CPPLanguageSelector::CurrentStructs> {};

template <>
struct LanguageSyntaxImpl<Language::CPP> final : LanguageSyntaxCPP<CPPLanguageSelector::NativeStructs> {};

template <>
struct LanguageSyntaxImpl<Language::FSharp> final {
  static std::string Header(const std::string&) {
    return "// Usage: `fsharpi -r Newtonsoft.Json.dll schema.fs`\n"
           "(*\n"
           "open Newtonsoft.Json\n"
           "let inline JSON o = JsonConvert.SerializeObject(o)\n"
           "let inline ParseJSON (s : string) : 'T = JsonConvert.DeserializeObject<'T>(s)\n"
           "*)\n";
  }

  static std::string Footer(const std::string&) { return ""; }

  struct FullSchemaPrinter final {
    const std::map<TypeID, ReflectedType>& types_;
    std::ostream& os_;
    mutable std::unordered_set<TypeID, CurrentHashFunction<TypeID>>
        empty_structs_;  // To not print the type of a DU case for empty structs.

    std::string TypeName(TypeID type_id) const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        return "UNKNOWN_TYPE_" + current::ToString(type_id);  // LCOV_EXCL_LINE
      } else {
        struct FSharpTypeNamePrinter final {
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
              oss_ << "UNKNOWN_BASIC_TYPE_" + current::ToString(p.type_id);  // LCOV_EXCL_LINE
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
          void operator()(const ReflectedType_Variant& v) const { oss_ << v.name; }
          void operator()(const ReflectedType_Struct& s) const { oss_ << s.CanonicalName(); }
        };

        std::ostringstream oss;
        cit->second.Call(FSharpTypeNamePrinter(*this, oss));
        return oss.str();
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types,
                      std::ostream& os,
                      const std::string&,
                      const Optional<NamespaceToExpose>&)
        : types_(types), os_(os) {}

    // `operator()`-s of this block print complete declarations of F# types.
    // The types that require complete declarations in F# are records and discriminated unions.
    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum& e) const {
      os_ << "\ntype " << e.name << " = " << TypeName(e.underlying_type) << '\n';
    }
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}
    void operator()(const ReflectedType_Variant& v) const {
      os_ << "\ntype " << v.name << " =\n";
      for (TypeID c : v.cases) {
        const auto name = TypeName(c);
        os_ << "| " << name;
        const auto& t = types_.at(c);
        assert(Exists<ReflectedType_Struct>(t) || Exists<ReflectedType_Variant>(t));  // Must be one of.
        if (!empty_structs_.count(Value<ReflectedTypeBase>(t).type_id)) {
          os_ << " of " << name;
        }
        os_ << '\n';
      }
    }

    // When dumping a `CURRENT_STRUCT` as an F# record, since inheritance is not supported by Newtonsoft.JSON,
    // all base class fields are hoisted to the top of the record.
    void RecursivelyListStructFieldsForFSharp(std::ostringstream& os, const ReflectedType_Struct& s) const {
      if (s.super_id != TypeID::CurrentStruct) {
        RecursivelyListStructFieldsForFSharp(os, Value<ReflectedType_Struct>(types_.at(s.super_id)));
      }
      bool first_field = true;
      for (const auto& f : s.fields) {
        if (Exists(f.description)) {
          if (!first_field) {
            os << '\n';
          }
          AppendAsMultilineCommentIndentedTwoSpaces(os, Value(f.description));
        }
        os << "  " << f.name << " : " << TypeName(f.type_id) << '\n';
        first_field = false;
      }
    }
    void operator()(const ReflectedType_Struct& s) const {
      std::ostringstream os;
      RecursivelyListStructFieldsForFSharp(os, s);
      const std::string fields = os.str();
      if (!fields.empty()) {
        os_ << "\ntype " << s.CanonicalName() << " = {\n" << fields << "}\n";
      } else {
        empty_structs_.insert(s.type_id);
      }
    }
  };  // struct LanguageSyntax<Language::FSharp>::FullSchemaPrinter

  // LCOV_EXCL_START
  static std::string ErrorMessageWithTypeId(TypeID type_id, FullSchemaPrinter&) {
    return "#error \"Unknown struct with `type_id` = " + current::ToString(type_id) + "\"\n";
  }
  // LCOV_EXCL_STOP
};

template <>
struct LanguageSyntaxImpl<Language::Markdown> final {
  static std::string Header(const std::string&) { return "# Data Dictionary\n"; }
  static std::string Footer(const std::string&) { return ""; }

  struct FullSchemaPrinter final {
    const std::map<TypeID, ReflectedType>& types_;
    std::ostream& os_;

    std::string TypeName(TypeID type_id) const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        return "UNKNOWN_TYPE_" + current::ToString(type_id);  // LCOV_EXCL_LINE
      } else {
        struct MarkdownTypeNamePrinter final {
          const FullSchemaPrinter& self_;
          std::ostringstream& oss_;

          MarkdownTypeNamePrinter(const FullSchemaPrinter& self, std::ostringstream& oss)
              : self_(self), oss_(oss) {}

          // `operator()(...)`-s of this block print the type name, without the expansion.
          void operator()(const ReflectedType_Primitive& p) const {
            const auto& globals = PrimitiveTypesList();
            if (globals.markdown_name.count(p.type_id) != 0u) {
              oss_ << globals.markdown_name.at(p.type_id);
            } else {
              oss_ << "UNKNOWN_BASIC_TYPE_" + current::ToString(p.type_id);  // LCOV_EXCL_LINE
            }
          }
          void operator()(const ReflectedType_Enum& e) const {
            oss_ << "Index `" << e.name << "`, underlying type `" << self_.TypeName(e.underlying_type) << '`';
          }
          void operator()(const ReflectedType_Vector& v) const {
            oss_ << "Array of " << self_.TypeName(v.element_type);
          }
          void operator()(const ReflectedType_Map& m) const {
            oss_ << "Map of " << self_.TypeName(m.key_type) << " into " << self_.TypeName(m.value_type);
          }
          void operator()(const ReflectedType_Pair& p) const {
            oss_ << "Pair of " << self_.TypeName(p.first_type) << " and " << self_.TypeName(p.second_type);
          }
          void operator()(const ReflectedType_Optional& o) const {
            oss_ << "`null` or " << self_.TypeName(o.optional_type);
          }
          void operator()(const ReflectedType_Variant& v) const {
            std::vector<std::string> cases;
            for (TypeID c : v.cases) {
              cases.push_back(self_.TypeName(c));
            }
            oss_ << "Algebraic " << current::strings::Join(cases, " / ") << " (a.k.a. `" << v.name << "`)";
          }

          void operator()(const ReflectedType_Struct& s) const { oss_ << '`' << s.CanonicalName() << '`'; }
        };

        std::ostringstream oss;
        cit->second.Call(MarkdownTypeNamePrinter(*this, oss));
        return oss.str();
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types,
                      std::ostream& os,
                      const std::string&,
                      const Optional<NamespaceToExpose>&)
        : types_(types), os_(os) {}

    // `operator()`-s of this block print complete declarations of F# types.
    // The types that require complete declarations in F# are records and discriminated unions.
    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum&) const {}
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}
    void operator()(const ReflectedType_Variant& v) const {
      std::vector<std::string> cases;
      for (auto c : v.cases) {
        cases.push_back(TypeName(c));  // Will be taken into tick quotes by itself.
      }
      os_ << "\n### `" << v.name << "`\nAlgebraic type, " << current::strings::Join(cases, " or ") << "\n\n";
    }

    // When dumping a derived `CURRENT_STRUCT` as a Markdown table, hoist base class fields to the top.
    void RecursivelyListStructFields(std::ostringstream& temporary_os, const ReflectedType_Struct& s) const {
      if (s.super_id != TypeID::CurrentStruct) {
        // TODO(dkorolev): Check that `at()` and `Value<>` succeeded.
        RecursivelyListStructFields(temporary_os, Value<ReflectedType_Struct>(types_.at(s.super_id)));
      }
      for (const auto& f : s.fields) {
        temporary_os << "| `" << f.name << "` | " << TypeName(f.type_id) << " |";
        if (Exists(f.description)) {
          temporary_os << ' ' << strings::EscapeForMarkdown(Value(f.description)) << " |";
        }
        temporary_os << '\n';
      }
    }

    void operator()(const ReflectedType_Struct& s) const {
      std::ostringstream temporary_os;
      RecursivelyListStructFields(temporary_os, s);
      const std::string fields = temporary_os.str();
      if (!fields.empty()) {
        os_ << "\n### `" << s.CanonicalName()
            << "`\n| **Field** | **Type** | **Description** |\n| ---: | :--- | :--- |\n" << fields << '\n';
      }
    }
  };  // struct LanguageSyntax<Language::Markdown>::FullSchemaPrinter

  // LCOV_EXCL_START
  static std::string ErrorMessageWithTypeId(TypeID type_id, FullSchemaPrinter&) {
    return "#error \"Unknown struct with `type_id` = " + current::ToString(type_id) + "\"\n";
  }
  // LCOV_EXCL_STOP
};

template <>
struct LanguageSyntaxImpl<Language::JSON> final {
  // The `Language::JSON` format ignores `Header()/Footer()`, all work is done by `struct FullSchemaPrinter`.
  static std::string Header(const std::string&) { return ""; }
  static std::string Footer(const std::string&) { return ""; }

  struct FullSchemaPrinter final {
    // Types to describe.
    const std::map<TypeID, ReflectedType>& types_;

    // Appended to while types are being enumerated.
    mutable JSONSchema schema_object_;

    // Filled from destructor.
    std::ostream& ostream_to_write_schema_to_;

    variant_clean_type_names::type_variant_t TypeDescriptionForJSON(TypeID type_id) const {
      const auto cit = types_.find(type_id);
      if (cit == types_.end()) {
        // LCOV_EXCL_START
        variant_clean_type_names::error error;
        error.error = "Unknown primitive type.";
        error.internal = type_id;
        return error;
        // LCOV_EXCL_STOP
      } else {
        struct JSONTypeExtractor final {
          const FullSchemaPrinter& self_;
          JSONSchema& schema_object_;
          mutable variant_clean_type_names::type_variant_t result_;

          JSONTypeExtractor(const FullSchemaPrinter& self, JSONSchema& output)
              : self_(self), schema_object_(output) {}

          // `operator()(...)`-s of this block fills in `this->result_` with the type, not expanding on it.
          void operator()(const ReflectedType_Primitive& p) const {
            const auto& globals = PrimitiveTypesList();
            if (globals.cpp_name.count(p.type_id) != 0u) {
              variant_clean_type_names::primitive result;
              result.type = globals.cpp_name.at(p.type_id);
              result.text = globals.markdown_name.at(p.type_id);
              result_ = result;
            } else {
              variant_clean_type_names::error error;
              error.error = "Unknown primitive type.";
              error.internal = p.type_id;
              result_ = error;
            }
          }
          void operator()(const ReflectedType_Enum& e) const {
            const auto& globals = PrimitiveTypesList();

            if (globals.cpp_name.count(e.underlying_type) != 0u) {
              variant_clean_type_names::key result;
              result.name = e.name;
              result.type = globals.cpp_name.at(e.underlying_type);
              result.text = globals.markdown_name.at(e.underlying_type);
              result_ = result;
            } else {
              variant_clean_type_names::error error;
              error.error = "Unknown key underlying type.";
              error.internal = e.underlying_type;
              result_ = error;
            }
          }
          void operator()(const ReflectedType_Vector& v) const {
            variant_clean_type_names::array result;
            result.element = self_.TypeDescriptionForJSON(v.element_type);
            result_ = result;
          }
          void operator()(const ReflectedType_Map& m) const {
            variant_clean_type_names::dictionary result;
            result.from = self_.TypeDescriptionForJSON(m.key_type);
            result.into = self_.TypeDescriptionForJSON(m.value_type);
            result_ = result;
          }
          void operator()(const ReflectedType_Pair& p) const {
            variant_clean_type_names::pair result;
            result.first = self_.TypeDescriptionForJSON(p.first_type);
            result.second = self_.TypeDescriptionForJSON(p.second_type);
            result_ = result;
          }
          void operator()(const ReflectedType_Optional& o) const {
            variant_clean_type_names::optional result;
            result.underlying = self_.TypeDescriptionForJSON(o.optional_type);
            result_ = result;
          }
          void operator()(const ReflectedType_Variant& p) const {
            variant_clean_type_names::algebraic result;
            for (TypeID c : p.cases) {
              result.cases.push_back(self_.TypeDescriptionForJSON(c));
            }
            result_ = result;
          }

          void operator()(const ReflectedType_Struct& s) const {
            variant_clean_type_names::object result;
            result.kind = s.CanonicalName();
            result_ = result;
          }
        };

        JSONTypeExtractor extractor(*this, schema_object_);
        cit->second.Call(extractor);
        assert(Exists(extractor.result_));  // Must be initialized by the above `.Call`.
        return extractor.result_;
      }
    }

    FullSchemaPrinter(const std::map<TypeID, ReflectedType>& types,
                      std::ostream& os,
                      const std::string&,
                      const Optional<NamespaceToExpose>&)
        : types_(types), ostream_to_write_schema_to_(os) {}

    ~FullSchemaPrinter() { ostream_to_write_schema_to_ << JSON<JSONFormat::Minimalistic>(schema_object_); }

    void operator()(const ReflectedType_Primitive&) const {}
    void operator()(const ReflectedType_Enum&) const {}
    void operator()(const ReflectedType_Vector&) const {}
    void operator()(const ReflectedType_Pair&) const {}
    void operator()(const ReflectedType_Map&) const {}
    void operator()(const ReflectedType_Optional&) const {}

    void operator()(const ReflectedType_Variant& v) const {
      for (auto& c : v.cases) {
        Reflector().ReflectedTypeByTypeID(c).Call(*this);
      }
    }

    // When dumping fields of a derived `CURRENT_STRUCT` as a JSON, hoist base class fields to the top.
    void RecursivelyListStructFieldsForJSON(JSONSchemaObject& object, const ReflectedType_Struct& s) const {
      if (s.super_id != TypeID::CurrentStruct) {
        // TODO(dkorolev): Check that `at()` and `Value<>` succeeded.
        RecursivelyListStructFieldsForJSON(object, Value<ReflectedType_Struct>(types_.at(s.super_id)));
      }
      for (const auto& f : s.fields) {
        JSONSchemaObjectField field;
        field.field = f.name;
        field.as = TypeDescriptionForJSON(f.type_id);
        field.description = f.description;
        object.contains.push_back(std::move(field));
      }
    }

    void operator()(const ReflectedType_Struct& s) const {
      JSONSchemaObject object;
      object.object = s.CanonicalName();
      RecursivelyListStructFieldsForJSON(object, s);
      schema_object_.push_back(std::move(object));
    }
  };  // struct LanguageSyntax<Language::JSON>::FullSchemaPrinter

  // LCOV_EXCL_START
  static std::string ErrorMessageWithTypeId(TypeID type_id, FullSchemaPrinter& printer) {
    // Report an error as part of the JSON, without crashing the application.
    variant_clean_type_names::error error;
    error.error = "Unknown top-level type.";
    error.internal = type_id;

    JSONSchemaObjectField field;
    field.field = "";
    field.as = error;

    JSONSchemaObject object;
    object.object = "";
    object.contains.push_back(std::move(field));

    printer.schema_object_.push_back(std::move(object));

    // Must return an empty string to make sure the JSON dumped from the destructor at the end is valid.
    return "";
  }
  // LCOV_EXCL_STOP
};

template <Language L>
struct LanguageDescribeCaller final {
  template <typename T>
  static std::string CallDescribe(const T* instance,
                                  bool headers,
                                  const Optional<NamespaceToExpose>& namespace_to_expose) {
    std::ostringstream oss;
    LanguageSyntax<L>::Describe(instance->types, instance->order, oss, headers, namespace_to_expose);
    return oss.str();
  }
};

// `SchemaInfo` contains information about all the types used in schema, their order
// and dependencies. It can describe the whole schema via `Describe()` method in any supported
// language. `SchemaInfo` acts independently, therefore can be easily saved/loaded using {de}serialization.
CURRENT_STRUCT(SchemaInfo) {
  // List of all type_id's contained in schema.
  CURRENT_FIELD(types, (std::map<TypeID, ReflectedType>));
  // The types in `order` are topologically sorted with respect to all directly and indirectly included types.
  CURRENT_FIELD(order, std::vector<TypeID>);

  CURRENT_DEFAULT_CONSTRUCTOR(SchemaInfo) {}
  CURRENT_CONSTRUCTOR(SchemaInfo)(const SchemaInfo& x) : types(x.types), order(x.order) {}
  CURRENT_CONSTRUCTOR(SchemaInfo)(SchemaInfo && x) : types(std::move(x.types)), order(std::move(x.order)) {}
  CURRENT_ASSIGN_OPER(SchemaInfo)(SchemaInfo const& x) {
    types = x.types;
    order = x.order;
    return *this;
  }
  CURRENT_ASSIGN_OPER(SchemaInfo)(SchemaInfo && x) {
    types = std::move(x.types);
    order = std::move(x.order);
    return *this;
  }

  template <Language L>
  std::string Describe(bool headers = true, const Optional<NamespaceToExpose>& namespace_to_expose = nullptr)
      const {
    return LanguageDescribeCaller<L>::CallDescribe(this, headers, namespace_to_expose);
  }
};

template <>
struct LanguageDescribeCaller<Language::InternalFormat> final {
  static std::string CallDescribe(const SchemaInfo* instance, bool, const Optional<NamespaceToExpose>&) {
    return JSON(*instance);
  }
};

struct StructSchema final {
  struct TypeTraverser final {
    TypeTraverser(SchemaInfo& schema) : schema_(schema) {}

    void operator()(const ReflectedType_Struct& s) {
      if (!schema_.types.count(s.type_id)) {
        // Fill `types[type_id]` before traversing everything else to break possible circular dependencies.
        schema_.types.emplace(s.type_id, s);
        if (s.super_id != TypeID::CurrentStruct) {
          Reflector().ReflectedTypeByTypeID(s.super_id).Call(*this);
        }
        for (const auto& f : s.fields) {
          Reflector().ReflectedTypeByTypeID(f.type_id).Call(*this);
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

    void operator()(const ReflectedType_Variant& v) {
      if (!schema_.types.count(v.type_id)) {
        schema_.types.emplace(v.type_id, v);
        for (TypeID c : v.cases) {
          Reflector().ReflectedTypeByTypeID(c).Call(*this);
        }
        schema_.order.push_back(v.type_id);
      }
    }

   private:
    SchemaInfo& schema_;
  };

  StructSchema() = default;
  StructSchema(const SchemaInfo& schema) : schema_(schema) {}
  StructSchema(SchemaInfo&& schema) : schema_(std::move(schema)) {}

  template <typename T>
  ENABLE_IF<!IS_CURRENT_STRUCT_OR_VARIANT(T)> AddType() {}

  template <typename T>
  ENABLE_IF<IS_CURRENT_STRUCT_OR_VARIANT(T)> AddType() {
    Reflector().ReflectType<T>().Call(TypeTraverser(schema_));
  }

  const SchemaInfo& GetSchemaInfo() const { return schema_; }

 private:
  SchemaInfo schema_;
};

}  // namespace reflection

// TODO(dkorolev) + TODO(mzhurovich): Unify the semantics for `ToString*<>` Current-wide.
namespace strings {
template <>
struct ToStringImpl<reflection::Language, false, true> final {
  static std::string DoIt(reflection::Language language) {
    switch (language) {
      case reflection::Language::InternalFormat:
        return "internal_json";
      case reflection::Language::Current:
        return "h";
      case reflection::Language::CPP:
        return "cpp";
      case reflection::Language::FSharp:
        return "fs";
      case reflection::Language::Markdown:
        return "md";
      case reflection::Language::JSON:
        return "json";
      default:
        return "language_code_" + ToString(static_cast<int>(language));
    }
  }
};
}  // namespace current::strings

}  // namespace current

#endif  // CURRENT_TYPE_SYSTEM_SCHEMA_SCHEMA_H
