/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_EXCEPTIONS_H
#define BRICKS_EXCEPTIONS_H

#include <exception>
#include <string>
#include <cstring>

#include "strings/printf.h"

namespace current {

class Exception : public std::exception {
 public:
  Exception() {}  // For Visual Studio's IntelliSense.
  Exception(const std::string& what) : what_(what) {}
  virtual ~Exception() = default;

  void SetWhat(const std::string& what) {
    what_ = what;
  }

  virtual std::string DetailedDescription() const noexcept {
    return strings::Printf("%s:%d", file_, line_) + '\t' + caller_ + '\t' + what_;
  }
  // LCOV_EXCL_START
  virtual const char* what() const noexcept override {
    constexpr size_t MAX_LENGTH = 1024 * 10;
    static char data[MAX_LENGTH + 1];
    const std::string message = DetailedDescription();
    if (message.length() < MAX_LENGTH) {
      std::strcpy(data, message.data());
    } else {
      std::copy(message.begin(), message.begin() + MAX_LENGTH, data);
      data[MAX_LENGTH] = '\0';
    }
    return data;
  }
  // LCOV_EXCL_STOP

  const std::string& OriginalWhat() const noexcept { return what_; }
  const char* File() const noexcept { return file_; }
  int Line() const noexcept { return line_; }
  const std::string& Caller() const noexcept { return caller_; }

  void SetOrigin(const char* file, int line) {
    file_ = file;
    line_ = line;
  }

  void SetCaller(const std::string& caller) { caller_ = caller; }

 private:
  std::string what_;
  const char* file_ = nullptr;
  int line_ = 0;
  std::string caller_;
};

// Extra parenthesis around `e((E))` are essential to not make it a function declaration.
#define CURRENT_THROW(E)             \
  {                                  \
    auto e((E));                     \
    e.SetCaller(#E);                 \
    e.SetOrigin(__FILE__, __LINE__); \
    throw e;                         \
  }

}  // namespace current

#endif  // BRICKS_EXCEPTIONS_H
