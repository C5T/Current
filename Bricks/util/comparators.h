/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
              2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BRICKS_UTIL_COMPARATOR_H
#define BRICKS_UTIL_COMPARATOR_H

#include <map>
#include <unordered_map>

namespace current {

namespace custom_comparator_and_hash_function {

template <typename T, bool IS_ENUM>
struct CurrentHashFunctionImpl;

template <typename T>
struct CurrentHashFunctionImpl<T, false> : std::hash<T> {};

template <typename T>
struct CurrentHashFunctionImpl<T, true> {
  std::size_t operator()(T x) const { return static_cast<size_t>(x); }
};

template <typename T, bool IS_ENUM>
struct CurrentComparatorImpl;

template <typename T>
struct CurrentComparatorImpl<T, false> : std::less<T> {};

template <typename T>
struct CurrentComparatorImpl<T, true> {
  bool operator()(T lhs, T rhs) const {
    using U = typename std::underlying_type<T>::type;
    return static_cast<U>(lhs) < static_cast<U>(rhs);
  }
};

}  // namespace custom_comparator_and_hash_function

template <typename KEY>
using CurrentHashFunction =
    custom_comparator_and_hash_function::CurrentHashFunctionImpl<KEY, std::is_enum<KEY>::value>;

template <typename KEY>
using CurrentComparator =
    custom_comparator_and_hash_function::CurrentComparatorImpl<KEY, std::is_enum<KEY>::value>;

}  // namespace current

#endif  // BRICKS_UTIL_COMPARATOR_H
