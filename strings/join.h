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

#ifndef BRICKS_STRINGS_JOIN_H
#define BRICKS_STRINGS_JOIN_H

#include <vector>
#include <string>
#include <type_traits>

#include "../template/rmref.h"

namespace bricks {
namespace strings {

namespace impl {

template <typename T>
struct StringLengthOrOneForCharImpl {};

template <>
struct StringLengthOrOneForCharImpl<std::string> {
  inline static size_t Length(const std::string& s) { return s.length(); }
};

template <size_t N>
struct StringLengthOrOneForCharImpl<const char[N]> {
  inline static typename std::enable_if<(N > 0), size_t>::type Length(const char[N]) { return N - 1; }
};

template <>
struct StringLengthOrOneForCharImpl<char> {
  inline static size_t Length(char) { return 1u; }
};

template <typename T>
inline size_t StringLengthOrOneForChar(T&& param) {
  return StringLengthOrOneForCharImpl<rmref<T>>::Length(param);
}

template <typename T_VECTOR_STRING, typename T_SEPARATOR>
std::string Join(const T_VECTOR_STRING& strings, T_SEPARATOR&& separator) {
  if (strings.empty()) {
    return "";
  } else {
    size_t length = 0;
    for (const auto cit : strings) {
      length += cit.length();
    }
    length += impl::StringLengthOrOneForChar(separator) * (strings.size() - 1);
    std::string result;
    result.reserve(length);
    bool first = true;
    for (const auto cit : strings) {
      if (first) {
        first = false;
      } else {
        result += separator;
      }
      result += cit;
    }
    return result;
  }
}

}  // namespace impl

template <typename T_VECTOR_STRING, typename T_SEPARATOR>
std::string Join(const T_VECTOR_STRING& strings, T_SEPARATOR&& separator) {
  return impl::Join(strings, separator);
}

template <typename T_SEPARATOR>
std::string Join(const std::vector<std::string>& strings, T_SEPARATOR&& separator) {
  return impl::Join(strings, separator);
}

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_JOIN_H
