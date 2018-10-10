/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2014 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
Copyright (c) 2018 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../port.h"

#include <cstdarg>
#include <string>
#include <vector>

namespace current {
namespace strings {

#ifdef __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
inline std::string Printf(const char *fmt, ...) {
  // Most of the platforms now support thread locals, so 64Kb buffer seems reasonable.
  constexpr int max_string_length_for_static_buffer = 64 * 1024 - 1;
  // Absolute limit on result size is 1Mb.
  constexpr int max_formatted_output_length = 1024 * 1024 - 1;

#ifdef CURRENT_HAS_THREAD_LOCAL
  thread_local static char buffer[max_string_length_for_static_buffer + 1];
#else
  // Slow but safe.
  char buffer[max_string_length_for_static_buffer + 1];
#endif

  va_list ap;
#ifndef CURRENT_WINDOWS
  va_start(ap, fmt);
  const int res = vsnprintf(buffer, max_string_length_for_static_buffer + 1, fmt, ap);
  va_end(ap);
  if (res > max_string_length_for_static_buffer) {
    const int large_buffer_length = std::min(res + 1, max_formatted_output_length + 1);
    std::vector<char> large_buffer(large_buffer_length);
    va_start(ap, fmt);
    vsnprintf(large_buffer.data(), large_buffer_length, fmt, ap);
    va_end(ap);
    return large_buffer.data();
  }
#else
  va_start(ap, fmt);
  const int res = _vsnprintf_s(buffer, max_string_length_for_static_buffer + 1, max_string_length_for_static_buffer, fmt, ap);
  va_end(ap);
  if (errno == ERANGE) {
    va_start(ap, fmt);
    const int large_buffer_length = std::min(_vscprintf(fmt, ap) + 1, max_formatted_output_length);
    va_end(ap);
    std::vector<char> large_buffer(large_buffer_length);
    va_start(ap, fmt);
    _vsnprintf_s(large_buffer.data(), large_buffer_length, _TRUNCATE, fmt, ap);
    va_end(ap);
    return large_buffer.data();
  }
#endif

  return buffer;
}

}  // namespace strings
}  // namespace current

using current::strings::Printf;

#endif  // BRICKS_STRINGS_PRINTF_H
