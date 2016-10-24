/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

// If this source file doesn't compile, run `regenerate.sh` to refresh the code in `golden/`.

#include "golden/schema_from.h"
#include "golden/schema_into.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"
#include "flags.h"

// `FullName` has changed. Need to compose the full name from first and last ones.
CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, FullName, into.full_name = from.last_name + ", " + from.first_name);

// `ShrinkingVariant` has changed. With three options going into two, need to fit the 3rd one into the 1st one.
CURRENT_VARIANT_EVOLVER(CustomEvolver, SchemaFrom, ShrinkingVariant, SchemaInto) {
  CURRENT_COPY_CASE(CustomTypeA);
  CURRENT_COPY_CASE(CustomTypeB);
  CURRENT_EVOLVE_CASE(CustomTypeC, {
    typename INTO::CustomTypeA value;
    value.a = from.c + 1;
    into = std::move(value);
  });
};

// `WithFieldsToRemove` has changed. Need to copy over `.foo` and `.bar`, and process `.baz`.
CURRENT_STRUCT_EVOLVER(CustomEvolver, SchemaFrom, WithFieldsToRemove, {
  CURRENT_COPY_FIELD(foo);
  CURRENT_COPY_FIELD(bar);
  if (!from.baz.empty()) {
    into.foo += ' ' + current::strings::Join(from.baz, ' ');
  }
});

TEST(TypeEvolution, SchemaFrom) {
  current::reflection::StructSchema struct_schema;
  current::reflection::NamespaceToExpose expose("SchemaFrom");

  struct_schema.AddType<typename SchemaFrom::TopLevel>();
  expose.template AddType<typename SchemaFrom::TopLevel>("ExposedTopLevel");

  // Compare the results to the golden files. Split-and-join for Windows-friendliness wrt. line endings. -- D.K.
  EXPECT_EQ(
      current::strings::Join(
          current::strings::Split<current::strings::ByLines>(
              struct_schema.GetSchemaInfo().Describe<current::reflection::Language::Current>(true, expose)),
          '\n'),
      current::strings::Join(current::strings::Split<current::strings::ByLines>(current::FileSystem::ReadFileAsString(
                                 current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_schema_from_file))),
                             '\n'));
}

TEST(TypeEvolution, SchemaInto) {
  current::reflection::StructSchema struct_schema;
  current::reflection::NamespaceToExpose expose("SchemaInto");

  struct_schema.AddType<typename SchemaInto::TopLevel>();
  expose.template AddType<typename SchemaInto::TopLevel>("ExposedTopLevel");

  // Compare the results to the golden files. Split-and-join for Windows-friendliness wrt. line endings. -- D.K.
  EXPECT_EQ(
      current::strings::Join(
          current::strings::Split<current::strings::ByLines>(
              struct_schema.GetSchemaInfo().Describe<current::reflection::Language::Current>(true, expose)),
          '\n'),
      current::strings::Join(current::strings::Split<current::strings::ByLines>(current::FileSystem::ReadFileAsString(
                                 current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_schema_into_file))),
                             '\n'));
}

TEST(TypeEvolution, Data) {
  // The golden data to populate.
  std::vector<std::string> golden_from;
  std::vector<std::string> golden_into;

  // Run type evolution.
  for (const std::string& line : current::strings::Split(
           current::FileSystem::ReadFileAsString(current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_data_from_file)),
           '\n')) {
    typename SchemaFrom::TopLevel from;
    typename SchemaInto::TopLevel into;

    ParseJSON(line, from);
    current::type_evolution::Evolve<SchemaFrom, typename SchemaFrom::TopLevel, current::type_evolution::CustomEvolver>::
        template Go<SchemaInto>(from, into);

    golden_from.push_back(JSON(from));
    golden_into.push_back(JSON(into));
  }

  // Compare the results to the golden files. Split-and-join for Windows-friendliness wrt. line endings. -- D.K.
  EXPECT_EQ(
      current::strings::Join(current::strings::Split<current::strings::ByLines>(current::FileSystem::ReadFileAsString(
                                 current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_data_from_file))),
                             '\n'),
      current::strings::Join(golden_from, '\n'));

  EXPECT_EQ(
      current::strings::Join(current::strings::Split<current::strings::ByLines>(current::FileSystem::ReadFileAsString(
                                 current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_data_into_file))),
                             '\n'),
      current::strings::Join(golden_into, '\n'));
}
