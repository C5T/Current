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

#include "../../typesystem/serialization/json.h"

#include "../../bricks/dflags/dflags.h"

#include "../../bricks/strings/printf.h"
#include "../../bricks/strings/split.h"

DEFINE_bool(header, false, "Set to treat the first row of the data as the header, and extract field names from it.");
DEFINE_string(separator, "\t", "The characters to use as separators in the input TSV/CSV file.");
DEFINE_bool(require_dense, true, "Set to false to allow some rows to be of fewer fields than others.");
DEFINE_string(na, "null:NA:N/A:", "Values that are to be treated as \"no value\". Note the empty string at the end.");
DEFINE_string(na_separators, ":", "The characters to split `--na` by.");

inline std::string ExcelColName(size_t i) {
  if (i < 26) {
    return std::string(1, 'A' + i);
  } else {
    return ExcelColName((i / 26) - 1) + static_cast<char>('A' + (i % 26));
  }
}

inline std::string MakeValidIdentifier(const std::string& s) {
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

  const auto na_values_parsed =
      current::strings::Split(FLAGS_na, FLAGS_na_separators, current::strings::EmptyFields::Keep);
  std::unordered_set<std::string> na_values(na_values_parsed.begin(), na_values_parsed.end());

  // Parse the input TSV/CVS by lines.
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

  // Populate field names, use Excel-like "A..Z,AA..ZZ,AAA..." or the ones from the header in `--header` mode.
  // Also confirm the TSV/CSV is dense (each row contains the same number of columns), unless `--require_dense false`.
  size_t total_columns = field_names.size();
  for (size_t i = 0; i < output.size(); ++i) {
    const auto& row = output[i];
    if (total_columns && total_columns != row.size() && FLAGS_require_dense) {
      std::cerr << "Data row of 1-based index " << (i + 1) << (FLAGS_header ? " (header discounted)" : "") << " has "
                << output[i].size() << " columns, while it should have " << total_columns << " ones." << std::endl;
      std::exit(-1);
    }
    total_columns = std::max(total_columns, row.size());
  }

  for (size_t j = field_names.size(); j < total_columns; ++j) {
    field_names.push_back(ExcelColName(j));
  }

  // Detect numerical columns.
  // NOTE(dkorolev): The code below makes no distinction between signed and unsigned int64-s.
  // The from-JSON schema inference tool would correctly mark unsigned values as `uint64_t`-s,
  // but input unsigned 64-bit values with the MSB set would be corrupted. -- D.K.
  enum class ColumnType : int { String = 0, Int64, Double };
  std::vector<ColumnType> column_type(total_columns, ColumnType::String);
  std::vector<std::vector<int64_t>> transposed_output_int64(total_columns);
  std::vector<std::vector<double>> transposed_output_double(total_columns);

  for (size_t j = 0; j < total_columns; ++j) {
    try {
      for (size_t i = 0; i < output.size(); ++i) {
        const auto& value = output[i][j];
        if (!na_values.count(value)) {
          if (output[i].size() > j) {
            transposed_output_int64[j].push_back(ParseJSON<int64_t>(value));
          } else {
            transposed_output_int64[j].push_back(0);
          }
        }
      }
      column_type[j] = ColumnType::Int64;
    } catch (const current::TypeSystemParseJSONException&) {
      // Not an `Int64` column.
    }
  }

  for (size_t j = 0; j < total_columns; ++j) {
    if (column_type[j] == ColumnType::String) {
      try {
        for (size_t i = 0; i < output.size(); ++i) {
          const auto& value = output[i][j];
          if (!na_values.count(value)) {
            if (output[i].size() > j) {
              transposed_output_double[j].push_back(ParseJSON<double>(value));
            } else {
              transposed_output_double[j].push_back(0);
            }
          }
        }
        column_type[j] = ColumnType::Double;
      } catch (const current::TypeSystemParseJSONException&) {
        // Not a `String` column.
      }
    }
  }

  // Generate and print the resulting JSON.
  // NOTE(dkorolev): Yes, this approach can be slow, esp. the "create in memory" part. Shouldn't matter now.
  rapidjson::Document json;
  json.SetArray();
  for (size_t i = 0; i < output.size(); ++i) {
    const auto& row = output[i];
    rapidjson::Value element;
    element.SetObject();
    for (size_t j = 0; j < row.size(); ++j) {
      if (!na_values.count(row[j])) {
        if (column_type[j] == ColumnType::String) {
          element.AddMember(rapidjson::StringRef(field_names[j]), rapidjson::StringRef(row[j]), json.GetAllocator());
        } else if (column_type[j] == ColumnType::Int64) {
          rapidjson::Value int64_value;
          int64_value.SetInt64(transposed_output_int64[j][i]);
          element.AddMember(rapidjson::StringRef(field_names[j]), std::move(int64_value), json.GetAllocator());
        } else if (column_type[j] == ColumnType::Double) {
          rapidjson::Value double_value;
          double_value.SetDouble(transposed_output_double[j][i]);
          element.AddMember(rapidjson::StringRef(field_names[j]), std::move(double_value), json.GetAllocator());
        } else {
          std::cerr << "Internal error." << std::endl;
          std::exit(-1);
        }
      } else {
        rapidjson::Value null_value;
        null_value.SetNull();
        element.AddMember(rapidjson::StringRef(field_names[j]), std::move(null_value), json.GetAllocator());
      }
    }
    json.PushBack(std::move(element), json.GetAllocator());
  }

  rapidjson::StringBuffer string_buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(string_buffer);
  json.Accept(writer);

  std::cout << string_buffer.GetString() << std::endl;
}
