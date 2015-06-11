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

#include "util.h"

#include "../template/decay.h"

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

template <bool CAN_GET_LENGTH>
struct OptionallyReserveOutputBufferImpl {
  template <typename T_CONTAINER, typename T_SEPARATOR>
  static void Impl(std::string&, const T_CONTAINER&, T_SEPARATOR&&) {}
};

template <>
struct OptionallyReserveOutputBufferImpl<true> {
  template <typename T_CONTAINER, typename T_SEPARATOR>
  static void Impl(std::string& output, const T_CONTAINER& components, T_SEPARATOR&& separator) {
    size_t length = 0;
    for (const auto cit : components) {
      length += cit.length();
    }
    length += impl::StringLengthOrOneForChar(separator) * (components.size() - 1);
    output.reserve(length);
  }
};

template <typename T_CONTAINER, typename T_SEPARATOR>
void OptionallyReserveOutputBuffer(std::string& output,
                                   const T_CONTAINER& components,
                                   T_SEPARATOR&& separator) {
  // TODO(dkorolev): SFINAE here. :-)
  OptionallyReserveOutputBufferImpl<false>::Impl(output, components, separator);
}

template <typename T_CONTAINER, typename T_SEPARATOR>
std::string Join(const T_CONTAINER& components, T_SEPARATOR&& separator) {
  if (components.empty()) {
    return "";
  } else {
    std::string result;
    OptionallyReserveOutputBuffer(result, components, separator);
    bool first = true;
    for (const auto cit : components) {
      if (first) {
        first = false;
      } else {
        result += separator;
      }
      result += ToString(cit);
    }
    return result;
  }
}

}  // namespace impl

template <typename T_CONTAINER, typename T_SEPARATOR>
std::string Join(const T_CONTAINER& components, T_SEPARATOR&& separator) {
  return impl::Join(components, std::forward<T_SEPARATOR>(separator));
}

template <typename T_SEPARATOR>
std::string Join(const std::vector<std::string>& components, T_SEPARATOR&& separator) {
  return impl::Join(components, std::forward<T_SEPARATOR>(separator));
}

}  // namespace strings
}  // namespace bricks

#endif  // BRICKS_STRINGS_JOIN_H
