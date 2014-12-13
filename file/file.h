// TODO(dkorolev): Add unit tests.

#ifndef BRICKS_FILE_FILE_H
#define BRICKS_FILE_FILE_H

/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Alexander Zolotarev <me@alex.bio> from Minsk, Belarus

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

#include <string>
#include <fstream>

#include "exceptions.h"

namespace bricks {

inline std::string ReadFileAsString(std::string const& file_name) {
  try {
    std::ifstream fi;
    fi.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fi.open(file_name, std::ifstream::in | std::ifstream::binary);
    fi.seekg(0, std::ios::end);
    const size_t size = fi.tellg();
    std::string buffer(size, '\0');
    fi.seekg(0);
    if (fi.read(&buffer[0], size).good()) {
      return buffer;
    } else {
      // TODO(dkorolev): Ask Alex whether there's a better way than what I have here with two exceptions.
      throw FileException();
    }
  } catch (const std::ifstream::failure&) {
    throw FileException();
  } catch (FileException()) {
    throw FileException();
  }
}

inline void WriteStringToFile(const std::string& file_name, const std::string& contents) {
  try {
    std::ofstream fo;
    fo.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    fo.open(file_name);
    fo << contents;
  } catch (const std::ofstream::failure&) {
    throw FileException();
  }
}

enum class RemoveFileParameters { ThrowExceptionOnError, Silent };
inline void RemoveFile(const std::string& file_name,
                       RemoveFileParameters parameters = RemoveFileParameters::ThrowExceptionOnError) {
  if (::remove(file_name.c_str())) {
    if (parameters == RemoveFileParameters::ThrowExceptionOnError) {
      throw FileException();
    }
  }
}

class ScopedRemoveFile final {
 public:
  explicit ScopedRemoveFile(const std::string& file_name, bool remove_now_as_well = true)
      : file_name_(file_name) {
    if (remove_now_as_well) {
      RemoveFile(file_name_, RemoveFileParameters::Silent);
    }
  }
  ~ScopedRemoveFile() {
    RemoveFile(file_name_, RemoveFileParameters::Silent);
  }

 private:
  std::string file_name_;
};

}  // namespace bricks

#endif  // BRICKS_FILE_FILE_H
