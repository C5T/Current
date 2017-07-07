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

#ifdef CURRENT_LOG_EVERY_THROW_TO_STDERR
#include <iostream>
#endif  // CURRENT_LOG_EVERY_THROW_TO_STDERR

#include <exception>
#include <string>
#include <cstring>

#include "strings/printf.h"

namespace current {

class Exception : public std::exception {
 public:
  Exception() {}  // For Visual Studio's IntelliSense.
  Exception(const std::string& message) : original_description_(message), detailed_description_(message) {}
  virtual ~Exception() = default;

  virtual std::string OriginalDescription() const noexcept { return original_description_; }

  virtual const std::string& DetailedDescription() const noexcept { return detailed_description_; }

  virtual const char* what() const noexcept override { return detailed_description_.c_str(); }  // LCOV_EXCL_LINE

  const char* File() const noexcept { return file_; }
  int Line() const noexcept { return line_; }
  const std::string& Caller() const noexcept { return caller_; }

  void FillDetails(const std::string& caller, const char* file, int line) {
    caller_ = caller;
    file_ = file;
    line_ = line;
    detailed_description_ = strings::Printf("%s:%d", file, line) + '\t' + caller + '\t' + original_description_;
  }

 private:
  // The user-defined description of the exception.
  std::string original_description_;

  // The description augmented with the full body of the invokation of `CURRENT_THROW`,
  // as well as `__FILE__` and `__LINE__`. This is what is returned by `what()`, `overriding std::exception::what()`.
  std::string detailed_description_;

  // Extra fields, adding which to `original_description_` results in `detailed_description_`.
  // Rationale:
  // 1) On the one hand, the overridden `what()` must return a `const char*`, and this dynamically constructing
  //    the full message describing the exception is undesirable.
  // 2) On the other hand, when running unit tests and a full top-level `make test`, the path to `__FILE__` may be
  //    relative rendering it useless for `EXPECT_EQ` checks. Testing the original message and `__LINE__` is still
  //    a good idea. Thus, we introduce this minor redundancy into the `current::Exception` class.
  const char* file_ = nullptr;
  int line_ = 0;
  std::string caller_;
};

// Extra parenthesis around `e((E))` are essential to not make it a function declaration.
// Also, call it `_e_` to avoid name collisions.
#ifndef CURRENT_LOG_EVERY_THROW_TO_STDERR
#define CURRENT_THROW(E)                     \
  {                                          \
    auto _e_((E));                           \
    _e_.FillDetails(#E, __FILE__, __LINE__); \
    throw _e_;                               \
  }
#else
#define CURRENT_THROW(E)                                                  \
  {                                                                       \
    std::cerr << #E << " @ " << __FILE__ << ':' << __LINE__ << std::endl; \
    auto _e_((E));                                                        \
    _e_.FillDetails(#E, __FILE__, __LINE__);                              \
    throw _e_;                                                            \
  }
#endif  // CURRENT_LOG_EVERY_THROW_TO_STDERR

}  // namespace current

#endif  // BRICKS_EXCEPTIONS_H
