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

#ifndef BRICKS_STRINGS_UTIL_H
#define BRICKS_STRINGS_UTIL_H

#include <cstddef>
#include <type_traits>

// These are small headers, and #include "bricks/strings/util.h" should be enough.
#include "printf.h"
#include "fixed_size_serializer.h"
#include "join.h"
#include "split.h"

namespace bricks {
namespace strings {

template <typename T>
inline std::string to_string(T&& something) {
  return std::to_string(something);
}

template <>
inline std::string to_string<std::string>(std::string&& shame_std_to_string_does_not_support_string) {
  return shame_std_to_string_does_not_support_string;
}

template <size_t N>
constexpr typename std::enable_if<(N > 0), size_t>::type CompileTimeStringLength(char const (&)[N]) {
  return N - 1;
}

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_UTIL_H
