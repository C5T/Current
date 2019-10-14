/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_STRINGS_IS_STRING_TYPE_H
#define BRICKS_STRINGS_IS_STRING_TYPE_H

#include <string>
#include <vector>

#include "../template/decay.h"

namespace current {
namespace strings {

// Helper compile-type test to tell string-like types from cerealizable types.
template <typename T>
struct is_string_type_impl {
  constexpr static bool value = false;
};
template <>
struct is_string_type_impl<std::string> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<char> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<char>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<int8_t>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<std::vector<uint8_t>> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<char*> {
  constexpr static bool value = true;
};
template <size_t N>
struct is_string_type_impl<char[N]> {
  constexpr static bool value = true;
};
template <>
struct is_string_type_impl<const char*> {
  constexpr static bool value = true;
};
template <size_t N>
struct is_string_type_impl<const char[N]> {
  constexpr static bool value = true;
};

template <typename T>
struct is_string_type {
  constexpr static bool value = is_string_type_impl<decay_t<T>>::value;
};

}  // namespace strings
}  // namespace current

#endif  // BRICKS_STRINGS_IS_STRING_TYPE_H
