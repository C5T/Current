/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2018 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_SYSTEM_SYSCALLS_H
#define BRICKS_SYSTEM_SYSCALLS_H

#include "../../port.h"
#include "../strings/util.h"

#include <cstdio>
#include <vector>

namespace current {
namespace bricks {
namespace system {

class InputTextPipe {
 private:
  std::vector<char> buffer;
  FILE* f;

 public:
  template <typename S>
  explicit InputTextPipe(S&& command, size_t max_line_length = 1024 * 1024)
      : buffer(max_line_length), f(::popen(strings::ConstCharPtr(std::forward<S>(command)), "r")) {
    if (!f) {
      //     CURRENT_THROW
    }
  }

  ~InputTextPipe() {
    if (f) {
      ::pclose(f);
    }
  }

  // Read a single line, or, if can not, returns an empty string.
  // Generally, if an empty string was returned from a valid `InputTextPipe`,
  // casting it to `bool` is the way to tell whether the stream of data to read has ended.
  std::string ReadLine() {
    if (f) {
      const char* p = ::fgets(&buffer[0], buffer.size(), f);
      if (!p) {
        ::pclose(f);
        f = nullptr;
        return "";
      } else {
        std::string s(p);
        if (!s.empty() && s.back() == '\n') {
          s.resize(s.length() - 1u);
        }
        return s;
      }
    } else {
      return "";
    }
  }
  // `false` if the `popen()` call failed, or is the previous `fgets()` call returned a null pointer.
  operator bool() const { return f; }
};

}  // namespace current::bricks::system
}  // namespace current::bricks
}  // namespace current

#endif  // BRICKS_SYSTEM_SYSCALLS_H
