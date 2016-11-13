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

#include <string>
#include <type_traits>
#include <vector>

#include "util.h"

#include "../template/decay.h"
#include "../template/enable_if.h"

namespace current {
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
  inline static ENABLE_IF<(N > 0), size_t> Length(const char[N]) { return N - 1; }
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
  template <typename CONTAINER, typename SEPARATOR>
  static void Impl(std::string&, const CONTAINER&, SEPARATOR&&) {}
};

template <>
struct OptionallyReserveOutputBufferImpl<true> {
  template <typename CONTAINER, typename SEPARATOR>
  static void Impl(std::string& output, const CONTAINER& components, SEPARATOR&& separator) {
    size_t length = 0;
    for (const auto& cit : components) {
      length += cit.length();
    }
    length += impl::StringLengthOrOneForChar(separator) * (components.size() - 1);
    output.reserve(length);
  }
};

namespace sfinae {

template <typename T>
struct HasBegin {
  static constexpr bool CompileTimeCheck(char) { return false; }
  static constexpr auto CompileTimeCheck(int) -> decltype(std::declval<T>().begin(), bool()) { return true; }
};

template <typename T, bool IS_CONTAINER>
struct IsContainerOfStringsImpl {};

template <typename T>
struct IsContainerOfStringsImpl<T, false> {
  enum { value = false };
};

template <typename T>
struct IsContainerOfStringsImpl<T, true> {
  enum { value = std::is_same<std::string, current::decay<decltype(*std::declval<T>().begin())>>::value };
};

template <typename T>
struct IsContainerOfStrings {
  enum { value = IsContainerOfStringsImpl<T, HasBegin<T>::CompileTimeCheck(0)>::value };
};

}  // namespace sfinae

template <typename CONTAINER, typename SEPARATOR>
void OptionallyReserveOutputBuffer(std::string& output, const CONTAINER& components, SEPARATOR&& separator) {
  // Note: this implementation does not do `reserve()` for chars, the length of which is always known to be 1.
  OptionallyReserveOutputBufferImpl<sfinae::IsContainerOfStrings<CONTAINER>::value>::Impl(
      output, components, separator);
}

template <typename CONTAINER, typename SEPARATOR>
std::string Join(const CONTAINER& components, SEPARATOR&& separator) {
  if (components.empty()) {
    return "";
  } else {
    std::string result;
    OptionallyReserveOutputBuffer(result, components, separator);
    bool first = true;
    for (const auto& cit : components) {
      if (first) {
        first = false;
      } else {
        result += separator;
      }
      result += current::ToString(cit);
    }
    return result;
  }
}

}  // namespace impl

template <typename CONTAINER, typename SEPARATOR>
std::string Join(const CONTAINER& components, SEPARATOR&& separator) {
  return impl::Join(components, std::forward<SEPARATOR>(separator));
}

template <typename SEPARATOR>
std::string Join(const std::vector<std::string>& components, SEPARATOR&& separator) {
  return impl::Join(components, std::forward<SEPARATOR>(separator));
}

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_JOIN_H
