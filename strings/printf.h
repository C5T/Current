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

#ifndef BRICKS_STRINGS_PRINTF_H
#define BRICKS_STRINGS_PRINTF_H

#include <string>
#include <cstdarg>

namespace bricks {
namespace strings {

inline std::string Printf(const char *fmt, ...) {
  const int max_formatted_output_length = 1024 * 1024;
  static char buf[max_formatted_output_length + 1];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, max_formatted_output_length, fmt, ap);
  va_end(ap);
  return buf;
}

}  // namespace string
}  // namespace bricks

#endif  // BRICKS_STRINGS_PRINTF_H
