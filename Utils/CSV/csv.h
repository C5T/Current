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

#ifndef CURRENT_UTILS_CSV_CSV_H
#define CURRENT_UTILS_CSV_CSV_H

#include "../../port.h"

#include <fstream>

#include "../../TypeSystem/struct.h"
#include "../../Bricks/strings/strings.h"
#include "../../Bricks/exception.h"

namespace current {

struct CSVException : Exception {
  using Exception::Exception;
};

struct CSVFileNotFoundException : CSVException {
  using CSVException::CSVException;
};

struct CSVFileFormatException : CSVException {
  using CSVException::CSVException;
};

CURRENT_STRUCT_T(CSV) {
  CURRENT_FIELD(header, std::vector<std::string>);
  CURRENT_FIELD(data, std::vector<std::vector<T>>);
  static CSV<T> ReadFile(const std::string& filename) {
    CSV<T> csv;
    std::ifstream fi(filename);
    if (!fi.good()) {
      CURRENT_THROW(CSVFileNotFoundException("The CSV file `" + filename + "` could not be opened."));
    }
    std::string line;
    if (!std::getline(fi, line)) {
      CURRENT_THROW(CSVFileFormatException("The CSV file `" + filename + "` is empty."));
    }
    if (!line.empty() && line.back() == '\r') {
      line.resize(line.size() - 1u);  // NOTE(dkorolev): This may look like a hack, but I'd rather stay safe.
    }
    csv.header = current::strings::Split(line, ',');
    if (csv.header.empty()) {
      CURRENT_THROW(CSVFileFormatException("The CSV file `" + filename + "` does not even contain the header."));
    }
    while (std::getline(fi, line)) {
      if (!line.empty() && line.back() == '\r') {
        line.resize(line.size() - 1u);  // NOTE(dkorolev): This may look like a hack, but I'd rather stay safe.
      }
      std::vector<T> row;
      for (const auto& field : current::strings::Split(line, ',')) {
        row.push_back(current::FromString<T>(field));
      }
      if (row.size() != csv.header.size()) {
        CURRENT_THROW(CSVFileFormatException("Column number mismatch in CSV file `" + filename + "`."));
      }
      csv.data.emplace_back(std::move(row));
    }
    return csv;
  }
};

}  // namespace current

#endif  // CURRENT_UTILS_CSV_CSV_H
