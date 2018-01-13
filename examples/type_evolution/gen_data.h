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

#ifndef EXAMPLES_TYPE_EVOLUTION_GEN_DATA_H
#define EXAMPLES_TYPE_EVOLUTION_GEN_DATA_H

#include "schema.h"

inline void GenerateData(const std::string& filename) {
  std::vector<std::string> records;

  records.push_back(JSON(TopLevel(std::integral_constant<int, 0>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 1>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 2>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 3>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 4>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 5>())));
  records.push_back(JSON(TopLevel(std::integral_constant<int, 6>())));

  current::FileSystem::WriteStringToFile(current::strings::Join(records, '\n'), filename.c_str());
}

#define GEN_DATA(realm)                                                                               \
  int main(int argc, char** argv) {                                                                   \
    ParseDFlags(&argc, &argv);                                                                        \
    current::FileSystem::MkDir(FLAGS_golden_dir, current::FileSystem::MkDirParameters::Silent);       \
    GenerateData(current::FileSystem::JoinPath(FLAGS_golden_dir, FLAGS_data_##realm##_file).c_str()); \
  }

#endif  // EXAMPLES_TYPE_EVOLUTION_GEN_DATA_H
