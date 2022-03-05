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
  inline static std::enable_if_t<(N > 0), size_t> Length(const char[N]) { return N - 1; }
};

template <>
struct StringLengthOrOneForCharImpl<char> {
  inline static size_t Length(char) { return 1u; }
};

template <typename T>
inline size_t StringLengthOrOneForChar(T&& param) {
  return StringLengthOrOneForCharImpl<std::remove_reference_t<T>>::Length(param);
}

namespace sfinae {

template <typename T, typename = void>
struct is_container : std::false_type {};

template <typename T>
struct is_container<
    T,
    std::void_t<decltype(std::declval<T>().begin()), decltype(std::declval<T>().end()), typename T::value_type>>
    : std::true_type {};

#ifndef CURRENT_FOR_CPP14

template <typename T>
constexpr bool is_container_of_strings() {
  if constexpr (is_container<T>::value) {
    return std::is_same_v<typename T::value_type, std::string>;
  }
  return false;
}

template <typename T>
constexpr bool is_container_of_strings_v = is_container_of_strings<T>();

#else

template <bool, typename>
struct is_container_of_strings_impl final {
  constexpr static bool value = false;
};

template <typename T>
struct is_container_of_strings_impl<true, T> final {
  constexpr static bool value = std::is_same_v<typename T::value_type, std::string>;
};

template <typename T>
constexpr bool is_container_of_strings_v = is_container_of_strings_impl<is_container<T>::value, T>::value;

#endif  // CURRENT_FOR_CPP14

}  // namespace sfinae

template <typename CONTAINER, typename SEPARATOR>
void OptionallyReserveOutputBuffer(std::string& output, const CONTAINER& components, SEPARATOR&& separator) {
#ifndef CURRENT_FOR_CPP14
  // Note: this implementation does not do `reserve()` for chars, the length of which is always known to be 1.
  if constexpr (sfinae::is_container_of_strings_v<CONTAINER>) {
    if (!components.empty()) {
      size_t length = 0;
      for (const auto& cit : components) {
        length += cit.length();
      }
      length += impl::StringLengthOrOneForChar(separator) * (components.size() - 1);
      output.reserve(length);
    }
  }
#else
  static_cast<void>(output);
  static_cast<void>(components);
  static_cast<void>(separator);
#endif  // CURRENT_FOR_CPP14
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
