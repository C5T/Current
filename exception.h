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

#include "strings/printf.h"

namespace bricks {

class Exception : public std::exception {
 public:
  Exception(const std::string& what = "") : what_(what) {}
  virtual ~Exception() = default;

  void SetWhat(const std::string& what) { what_ = what; }

  // LCOV_EXCL_START
  virtual const char* what() const noexcept override { return what_.c_str(); }
  // LCOV_EXCL_STOP

  virtual const std::string& What() const noexcept { return what_; }

  void SetCaller(const std::string& caller) { what_ = caller + '\t' + what_; }

  void SetOrigin(const char* file, int line) { what_ = strings::Printf("%s:%d\t", file, line) + what_; }

 private:
  std::string what_;
};

// Extra parenthesis around `e((E))` are essential to not make it a function declaration.
#define BRICKS_THROW(E)              \
  {                                  \
    auto e((E));                     \
    e.SetCaller(#E);                 \
    e.SetOrigin(__FILE__, __LINE__); \
    throw e;                         \
  }

}  // namespace bricks

#endif  // BRICKS_EXCEPTIONS_H
