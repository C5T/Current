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

#ifndef EXAMPLES_TYPE_EVOLUTION_GEN_SCHEMA_H
#define EXAMPLES_TYPE_EVOLUTION_GEN_SCHEMA_H

#include "schema.h"

inline void GenerateSchema(const std::string& filename, const std::string& exposed_namespace_name) {
  current::reflection::StructSchema struct_schema;
  current::reflection::NamespaceToExpose expose(exposed_namespace_name);

  struct_schema.AddType<TopLevel>();
  expose.template AddType<TopLevel>("ExposedTopLevel");

  current::FileSystem::WriteStringToFile(
      struct_schema.GetSchemaInfo().Describe<current::reflection::Language::Current>(true, expose), filename.c_str());
}

#define GEN_SCHEMA(realm, Realm)                                                                                 \
  int main(int argc, char** argv) {                                                                              \
    ParseDFlags(&argc, &argv);                                                                                   \
    current::FileSystem::MkDir(FLAGS_golden_dir, current::FileSystem::MkDirParameters::Silent);                  \
    GenerateSchema(current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_schema_##realm##_file).c_str(), Realm); \
  }

#endif  // EXAMPLES_TYPE_EVOLUTION_GEN_SCHEMA_H
