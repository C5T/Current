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

#include "../../TypeSystem/Serialization/json/rapidjson.h"

#include "../../Bricks/dflags/dflags.h"

#include "../../Bricks/strings/printf.h"
#include "../../Bricks/strings/split.h"

DEFINE_bool(header, false, "Set to treat the first row of the data as the header, and extract field names from it.");
DEFINE_string(separator, "\t", "The characters to use as separators in the input TSV/CSV file.");
DEFINE_bool(require_dense, true, "Set to false to allow some rows to be of fewer fields than others.");

std::string ExcelColName(size_t i) {
  if (i < 26) {
    return std::string(1, 'A' + i);
  } else {
    return ExcelColName((i / 26) - 1) + static_cast<char>('A' + (i % 26));
  }
}

std::string MakeValidIdentifier(const std::string& s) {
  if (s.empty()) {
    return "_";
  } else {
    std::string result;
    for (char c : s) {
      if (std::isalnum(c)) {
        result += c;
      } else {
        result += current::strings::Printf("x%02X", static_cast<int>(static_cast<unsigned char>(c)));
      }
    }
    return std::isalpha(result.front()) ? result : "_" + result;
  }
}

int main(int argc, char** argv) {
  ParseDFlags(&argc, &argv);
  std::vector<std::vector<std::string>> output;
  std::string row_as_string;
  bool header_to_parse = FLAGS_header;
  std::vector<std::string> field_names;
  while (std::getline(std::cin, row_as_string)) {
    if (!row_as_string.empty()) {
      const std::vector<std::string> fields =
          current::strings::Split(row_as_string, FLAGS_separator, current::strings::EmptyFields::Keep);
      if (header_to_parse) {
        std::unordered_map<std::string, size_t> key_counters;  // To convert "X,X,X" into "X,X2,X3".
        for (const std::string& field : fields) {
          std::string key = MakeValidIdentifier(field);
          const size_t index = ++key_counters[key];
          if (index > 1) {
            // NOTE(dkorolev): Yes, this may create duplicates. For now it's by design.
            key += current::ToString(index);
          }
          field_names.emplace_back(std::move(key));
        }
        header_to_parse = false;
      } else {
        output.emplace_back(std::move(fields));
      }
    }
  }

  size_t total_cols = field_names.size();
  for (size_t i = 0; i < output.size(); ++i) {
    const auto& row = output[i];
    if (total_cols && total_cols != row.size() && FLAGS_require_dense) {
      std::cerr << "Data row of 1-based index " << (i + 1) << (FLAGS_header ? " (header discounted)" : "") << " has "
                << output[i].size() << " columns, while it should have " << total_cols << " ones." << std::endl;
      std::exit(-1);
    }
    total_cols = std::max(total_cols, row.size());
  }

  for (size_t i = field_names.size(); i < total_cols; ++i) {
    field_names.push_back(ExcelColName(i));
  }

  rapidjson::Document json;
  json.SetArray();
  for (const auto& row : output) {
    rapidjson::Value element;
    element.SetObject();
    for (size_t i = 0; i < row.size(); ++i) {
      element.AddMember(rapidjson::StringRef(field_names[i]), rapidjson::StringRef(row[i]), json.GetAllocator());
    }
    json.PushBack(std::move(element), json.GetAllocator());
  }

  rapidjson::StringBuffer string_buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
  json.Accept(writer);

  std::cout << string_buffer.GetString() << std::endl;
}
