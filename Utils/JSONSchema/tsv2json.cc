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

#include <iostream>
#include <string>
#include <sstream>

#include "../../TypeSystem/Serialization/json.h"

#include "../../Bricks/strings/printf.h"
#include "../../Bricks/strings/split.h"

std::string ExcelColName(size_t i) {
  if (i < 26) {
    return std::string(1, 'A' + i);
  } else {
    return ExcelColName((i / 26) - 1) + static_cast<char>('A' + (i % 26));
  }
}

int main() {
  std::vector<std::map<std::string, std::string>> output;
  std::string row_as_string;
  while (std::getline(std::cin, row_as_string)) {
    if (!row_as_string.empty()) {
      const std::vector<std::string> fields = current::strings::Split(row_as_string, '\t', current::strings::EmptyFields::Keep);
      std::map<std::string, std::string> row;
      for (size_t i = 0; i < fields.size(); ++i) {
        row[ExcelColName(i)] = fields[i];
      }
      output.emplace_back(std::move(row));
    }
  }
  std::cout << JSON(output) << std::endl;
}
