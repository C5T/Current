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

// This `test.cc` file is `#include`-d from `../test.cc`, and thus needs a header guard.

#ifndef CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC
#define CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC

#include "schema.h"

#include "../Serialization/json.h"
#include "../Evolution/type_evolution.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/strings/strings.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_bool(write_reflection_golden_files, false, "Set to true to [over]write the golden files.");

namespace schema_test {

// clang-format off

CURRENT_ENUM(Enum, uint32_t) {};

CURRENT_STRUCT(X) {
  CURRENT_FIELD(i, int32_t);
};

CURRENT_STRUCT(Y) {
  CURRENT_FIELD(v, std::vector<X>);
};

CURRENT_STRUCT(Z, Y) {
  CURRENT_FIELD(d, double);
  CURRENT_FIELD(v2, std::vector<std::vector<Enum>>);
};

CURRENT_STRUCT(A) {
  CURRENT_FIELD(i, uint32_t);
};

CURRENT_STRUCT(B) {
  CURRENT_FIELD(x, X);
  CURRENT_FIELD(a, A);
};

CURRENT_STRUCT(C) {
  CURRENT_FIELD(b, Optional<B>);
};

CURRENT_VARIANT(NamedVariantX, X);  // using NamedVariantX = Variant<X>;
CURRENT_VARIANT(NamedVariantXY, X, Y);  // using NamedVariantXY = Variant<X, Y>;

// clang-format on

}  // namespace schema_test

TEST(Schema, StructSchema) {
  using namespace schema_test;
  using current::reflection::SchemaInfo;
  using current::reflection::StructSchema;
  using current::reflection::Language;

  StructSchema struct_schema;
  {
    const SchemaInfo schema = struct_schema.GetSchemaInfo();
    EXPECT_TRUE(schema.order.empty());
    EXPECT_TRUE(schema.types.empty());
    EXPECT_EQ(
        "namespace current_userspace {\n"
        "}  // namespace current_userspace\n",
        schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<uint64_t>();
  struct_schema.AddType<double>();
  struct_schema.AddType<std::string>();

  {
    const SchemaInfo schema = struct_schema.GetSchemaInfo();
    EXPECT_TRUE(schema.order.empty());
    EXPECT_TRUE(schema.types.empty());
    EXPECT_EQ(
        "namespace current_userspace {\n"
        "}  // namespace current_userspace\n",
        schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<Z>();

  {
    const SchemaInfo schema = struct_schema.GetSchemaInfo();
    EXPECT_EQ(
        "namespace current_userspace {\n"
        "struct X {\n"
        "  int32_t i;\n"
        "};\n"
        "struct Y {\n"
        "  std::vector<X> v;\n"
        "};\n"
        "enum class Enum : uint32_t {};\n"
        "struct Z : Y {\n"
        "  double d;\n"
        "  std::vector<std::vector<Enum>> v2;\n"
        "};\n"
        "}  // namespace current_userspace\n",

        schema.Describe<Language::CPP>(false));
  }

  struct_schema.AddType<C>();

  {
    const SchemaInfo schema = struct_schema.GetSchemaInfo();
    EXPECT_EQ(
        "namespace current_userspace {\n"
        "struct X {\n"
        "  int32_t i;\n"
        "};\n"
        "struct Y {\n"
        "  std::vector<X> v;\n"
        "};\n"
        "enum class Enum : uint32_t {};\n"
        "struct Z : Y {\n"
        "  double d;\n"
        "  std::vector<std::vector<Enum>> v2;\n"
        "};\n"
        "struct A {\n"
        "  uint32_t i;\n"
        "};\n"
        "struct B {\n"
        "  X x;\n"
        "  A a;\n"
        "};\n"
        "struct C {\n"
        "  Optional<B> b;\n"
        "};\n"
        "}  // namespace current_userspace\n",
        schema.Describe<Language::CPP>(false));
  }
}

TEST(Schema, VariantAloneIsTraversed) {
  using namespace schema_test;
  using current::reflection::SchemaInfo;
  using current::reflection::StructSchema;
  using current::reflection::Language;

  StructSchema struct_schema;

  struct_schema.AddType<Variant<A, X, Y>>();

  {
    const SchemaInfo schema = struct_schema.GetSchemaInfo();
    EXPECT_EQ(
        "namespace current_userspace {\n"
        "struct A {\n"
        "  uint32_t i;\n"
        "};\n"
        "struct X {\n"
        "  int32_t i;\n"
        "};\n"
        "struct Y {\n"
        "  std::vector<X> v;\n"
        "};\n"
        "using Variant_B_A_X_Y_E = Variant<A, X, Y>;\n"
        "}  // namespace current_userspace\n",
        schema.Describe<Language::CPP>(false));
  }
}

namespace schema_test {

// clang-format off

CURRENT_STRUCT(SelfContainingA) {
  CURRENT_FIELD(v, std::vector<SelfContainingA>);
};

CURRENT_STRUCT(SelfContainingB) {
  CURRENT_FIELD(v, std::vector<SelfContainingB>);
};

CURRENT_STRUCT(SelfContainingC, SelfContainingA) {
  CURRENT_FIELD(v, std::vector<SelfContainingB>);
  CURRENT_FIELD(m, (std::map<std::string, SelfContainingC>));
};

// clang-format on

}  // namespace schema_test

TEST(Schema, SelfContainingStruct) {
  using namespace schema_test;
  using current::reflection::StructSchema;
  using current::reflection::SchemaInfo;
  using current::reflection::Language;

  StructSchema struct_schema;
  struct_schema.AddType<SelfContainingC>();

  const SchemaInfo schema = struct_schema.GetSchemaInfo();
  EXPECT_EQ(
      "namespace current_userspace {\n"
      "struct SelfContainingA {\n"
      "  std::vector<SelfContainingA> v;\n"
      "};\n"
      "struct SelfContainingB {\n"
      "  std::vector<SelfContainingB> v;\n"
      "};\n"
      "struct SelfContainingC : SelfContainingA {\n"
      "  std::vector<SelfContainingB> v;\n"
      "  std::map<std::string, SelfContainingC> m;\n"
      "};\n"
      "}  // namespace current_userspace\n",
      schema.Describe<Language::CPP>(false));
}

#define SMOKE_TEST_STRUCT_NAMESPACE smoke_test_struct_namespace
#include "smoke_test_struct.h"
#undef SMOKE_TEST_STRUCT_NAMESPACE

TEST(Schema, SmokeTestFullStruct) {
  using current::FileSystem;
  using current::reflection::StructSchema;
  using current::reflection::SchemaInfo;
  using current::reflection::Language;

  const auto Golden = [](const std::string& s) { return FileSystem::JoinPath("golden", s); };
  const auto Serialized = [](const std::string& s) { return FileSystem::JoinPath("serialized", s); };

  StructSchema struct_schema;
  struct_schema.AddType<smoke_test_struct_namespace::FullTest>();
  const SchemaInfo schema = struct_schema.GetSchemaInfo();

  current::reflection::NamespaceToExpose namespace_to_expose("ExposedNamespace");
  namespace_to_expose.template AddType<smoke_test_struct_namespace::Empty>("ExposedEmpty");
  namespace_to_expose.template AddType<smoke_test_struct_namespace::Primitives>("ExposedPrimitives");
  namespace_to_expose.template AddType<smoke_test_struct_namespace::FullTest>("ExposedFullTest");

  if (FLAGS_write_reflection_golden_files) {
    // LCOV_EXCL_START
    FileSystem::WriteStringToFile(schema.Describe<Language::Current>(namespace_to_expose),
                                  Golden("smoke_test_struct.h").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::CPP>(), Golden("smoke_test_struct.cc").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::FSharp>(), Golden("smoke_test_struct.fs").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::Markdown>(), Golden("smoke_test_struct.md").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::JSON>(), Golden("smoke_test_struct.json").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::InternalFormat>(),
                                  Golden("smoke_test_struct.internal_json").c_str());
    FileSystem::WriteStringToFile(schema.Describe<Language::TypeScript>(), Golden("smoke_test_struct.ts").c_str());

    // TODO(sompylasar): More populated initialization of `full_test`.
    const smoke_test_struct_namespace::FullTest full_test{smoke_test_struct_namespace::B2()};
    FileSystem::WriteStringToFile(JSON(full_test), Serialized("smoke_test_struct.json").c_str());
    // LCOV_EXCL_STOP
  }

  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.h")),
            schema.Describe<Language::Current>(namespace_to_expose));
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.cc")), schema.Describe<Language::CPP>());
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.fs")), schema.Describe<Language::FSharp>());
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.md")), schema.Describe<Language::Markdown>());
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.ts")), schema.Describe<Language::TypeScript>());

  // Don't just `EXPECT_EQ(golden, ReadFileAsString("golden/...))`, but compare re-generated JSON,
  // as the JSON file in the golden directory is pretty-printed.
  const auto restored_schema =
      ParseJSON<SchemaInfo>(FileSystem::ReadFileAsString(Golden("smoke_test_struct.internal_json")));
  EXPECT_EQ(JSON(restored_schema), JSON(struct_schema.GetSchemaInfo()));

  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.h")),
            restored_schema.Describe<Language::Current>(namespace_to_expose));
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.cc")), restored_schema.Describe<Language::CPP>());
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.fs")), restored_schema.Describe<Language::FSharp>());
  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.md")),
            restored_schema.Describe<Language::Markdown>());

  // Don't just `EXPECT_EQ(golden, ReadFileAsString("golden/...))`, but compare re-generated JSON,
  // as the JSON file in the golden directory is pretty-printed.
  const auto restored_short_schema = ParseJSON<current::reflection::JSONSchema, JSONFormat::Minimalistic>(
      FileSystem::ReadFileAsString(Golden("smoke_test_struct.json")));
  EXPECT_EQ(schema.Describe<Language::JSON>(), JSON<JSONFormat::Minimalistic>(restored_short_schema));

  EXPECT_EQ(FileSystem::ReadFileAsString(Golden("smoke_test_struct.ts")),
            restored_schema.Describe<Language::TypeScript>());
}

namespace schema_test {

CURRENT_STRUCT(FS) {
  CURRENT_FIELD(type, std::string);
  CURRENT_FIELD(method, int64_t);
  CURRENT_FIELD(foo, bool);
};

}  // namespace schema_test

TEST(Schema, FSharpSanitization) {
  using namespace schema_test;
  using current::reflection::SchemaInfo;
  using current::reflection::StructSchema;
  using current::reflection::Language;

  StructSchema struct_schema;

  struct_schema.AddType<FS>();

  const SchemaInfo schema = struct_schema.GetSchemaInfo();
  EXPECT_EQ(
      "\n"
      "type FS = {\n"
      "  ``type`` : string\n"
      "  ``method`` : int64\n"
      "  foo : bool\n"
      "}\n",
      schema.Describe<Language::FSharp>(false));
}

namespace schema_test {

// Manual tweak to make it legal to `#include` an autogenerated header file from within a namespace.
// NOTE: This inner `namespace current` should be in the last `namespace schema_test` clause within this file,
// otherwise ambiguity issues for namespace `current` would come into play. -- D.K.
namespace current {
namespace type_evolution {
template <typename FROM_NAMESPACE, typename FROM_TYPE, typename EVOLVER = ::current::type_evolution::NaturalEvolver>
struct Evolve;
}  // namespace schema_test::current::type_evolution
}  // namespace schema_test::current

#include "golden/smoke_test_struct.h"
}  // namespace schema_test

TEST(Schema, SmokeTestFullStructCompiledFromAutogeneratedCode) {
  using namespace current::reflection;

  const auto Golden = [](const std::string& s) { return current::FileSystem::JoinPath("golden", s); };

  // Must expose same types under the same names within the same exposed namespace.
  const auto namespace_to_expose = current::reflection::NamespaceToExpose("ExposedNamespace")
                                       .template AddType<schema_test::ExposedNamespace::Empty>("ExposedEmpty")
                                       .template AddType<schema_test::ExposedNamespace::Primitives>("ExposedPrimitives")
                                       .template AddType<schema_test::ExposedNamespace::FullTest>("ExposedFullTest");

  // Verify the header is the right one. If fails, run `./current/test --write_reflection_golden_files`.
  {
    StructSchema old_struct_schema;
    old_struct_schema.AddType<schema_test::ExposedNamespace::FullTest>();
    const SchemaInfo old_schema = old_struct_schema.GetSchemaInfo();
    EXPECT_EQ(current::FileSystem::ReadFileAsString(Golden("smoke_test_struct.h")),
              old_schema.Describe<Language::Current>(namespace_to_expose));
  }

  // Verify the reflection of the type `#include`-d from its auto-generated header is the right one.
  // It is essential to confirm all type IDs stay the same, including the ones from `CURRENT_STRUCT_T`-s.
  {
    StructSchema new_struct_schema;
    new_struct_schema.AddType<schema_test::ExposedNamespace::FullTest>();
    const SchemaInfo new_schema = new_struct_schema.GetSchemaInfo();

#ifdef CURRENT_WINDOWS
    if (FLAGS_write_reflection_golden_files) {
#else
    if (true) {
#endif
      current::FileSystem::WriteStringToFile(new_schema.Describe<Language::Current>(namespace_to_expose),
                                             ".current_regenerated_schema.h");
    }
    EXPECT_EQ(current::FileSystem::ReadFileAsString(Golden("smoke_test_struct.h")),
              new_schema.Describe<Language::Current>(namespace_to_expose))
        << "diff '" << Golden("smoke_test_struct.h") << "' '.current_regenerated_schema.h'";
  }
}

TEST(Schema, LanguageEnumToString) {
  EXPECT_EQ("internal_json", current::ToString(current::reflection::Language::InternalFormat));
  EXPECT_EQ("h", current::ToString(current::reflection::Language::Current));
  EXPECT_EQ("cpp", current::ToString(current::reflection::Language::CPP));
  EXPECT_EQ("fs", current::ToString(current::reflection::Language::FSharp));
  EXPECT_EQ("md", current::ToString(current::reflection::Language::Markdown));
  EXPECT_EQ("json", current::ToString(current::reflection::Language::JSON));
  EXPECT_EQ("ts", current::ToString(current::reflection::Language::TypeScript));
}

TEST(Schema, LanguageEnumIteration) {
  using current::reflection::Language;
  std::vector<std::string> s;
  for (auto l = Language::begin; l != Language::end; ++l) {
    s.push_back(current::ToString(l));
  }
  EXPECT_EQ("internal_json h cpp fs md json ts", current::strings::Join(s, ' '));
}

namespace schema_test {
struct LanguagesIterator {
  std::vector<std::string> s;
  template <::current::reflection::Language language_as_compile_time_parameter>
  void PerLanguage() {
    s.push_back(::current::ToString(language_as_compile_time_parameter));
  }
};
}  // namespace schema_test

TEST(Schema, LanguageEnumCompileTimeForEach) {
  auto it = schema_test::LanguagesIterator();
  EXPECT_EQ("", current::strings::Join(it.s, ' '));
  current::reflection::ForEachLanguage(it);
  EXPECT_EQ("internal_json h cpp fs md json ts", current::strings::Join(it.s, ' '));
}

#define SMOKE_TEST_TEMPLATES_NAMESPACE smoke_test_templates_namespace_native
#include "smoke_test_templates.h"
#undef SMOKE_TEST_TEMPLATES_NAMESPACE

namespace smoke_test_templates_namespace_autogenerated {
// Manual tweak to make it legal to `#include` an autogenerated header file from within a namespace.
// NOTE: This inner `namespace current` should be in the last `namespace schema_test` clause within this file,
// otherwise ambiguity issues for namespace `current` would come into play. -- D.K.
namespace current {
namespace type_evolution {
template <typename FROM_NAMESPACE, typename FROM_TYPE, typename EVOLVER = ::current::type_evolution::NaturalEvolver>
struct Evolve;
}  // namespace schema_test::current::type_evolution
}  // namespace schema_test::current

#include "golden/smoke_test_template.h"  // The `golden/smoke_test_template{"","_2","3"}.h` files are identical.
}  // namespace smoke_test_templates_namespace_autogenerated

template <typename T>
std::string TemplatedStructSchemaAsCPP() {
  using current::reflection::StructSchema;
  using current::reflection::SchemaInfo;
  using current::reflection::Language;
  using current::reflection::NamespaceToExpose;

  StructSchema struct_schema;
  struct_schema.AddType<T>();
  const SchemaInfo schema = struct_schema.GetSchemaInfo();
  NamespaceToExpose namespace_to_expose("ExposedTemplates");
  namespace_to_expose.template AddType<T>("TestExposure");
  return schema.Describe<Language::Current>(namespace_to_expose);
}

TEST(Schema, SmokeTestTemplateExposure) {
  using current::FileSystem;

  using namespace smoke_test_templates_namespace_native;
  using autogenerated_t = typename smoke_test_templates_namespace_autogenerated::ExposedTemplates::TestExposure;

  EXPECT_STREQ("Templated_Z", current::reflection::CurrentTypeName<Templated<Integer>>());
  EXPECT_STREQ("Templated_Z", current::reflection::CurrentTypeName<no_template::FakeTemplatedInteger>());
  EXPECT_STREQ("Templated_Z", current::reflection::CurrentTypeName<autogenerated_t>());

  EXPECT_EQ(static_cast<uint64_t>(current::reflection::CurrentTypeID<Templated<Integer>>()),
            static_cast<uint64_t>(current::reflection::CurrentTypeID<no_template::FakeTemplatedInteger>()));

  const auto Golden = [](const std::string& s) { return FileSystem::JoinPath("golden", s); };

  const auto s1 = TemplatedStructSchemaAsCPP<Templated<Integer>>();
  const auto s2 = TemplatedStructSchemaAsCPP<no_template::FakeTemplatedInteger>();
  const auto s3 = TemplatedStructSchemaAsCPP<autogenerated_t>();

  EXPECT_EQ(s1, s2);
  EXPECT_EQ(s1, s3);

  if (FLAGS_write_reflection_golden_files) {
    // Mostly for `diff`-s if or when debugging is required. -- D.K.
    current::FileSystem::WriteStringToFile(s1, Golden("smoke_test_template.h").c_str());
    current::FileSystem::WriteStringToFile(s2, Golden("smoke_test_template_2.h").c_str());
    current::FileSystem::WriteStringToFile(s3, Golden("smoke_test_template_3.h").c_str());
  } else {
    EXPECT_EQ(s1, current::FileSystem::ReadFileAsString(Golden("smoke_test_template.h")));
    EXPECT_EQ(s2, current::FileSystem::ReadFileAsString(Golden("smoke_test_template_2.h")));
    EXPECT_EQ(s3, current::FileSystem::ReadFileAsString(Golden("smoke_test_template_3.h")));
  }
}

#endif  // CURRENT_TYPE_SYSTEM_SCHEMA_TEST_CC
