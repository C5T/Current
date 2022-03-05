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

#include "../../port.h"

#include <chrono>
#include <map>
#include <unordered_map>

namespace current {

// Hash combine function.
// Source: http://www.boost.org/doc/libs/1_55_0/doc/html/hash/reference.html#boost.hash_combine
template <typename T, class HASH = std::hash<T>>
inline void HashCombine(std::size_t& seed, const T& v) {
  seed ^= HASH()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace custom_comparator_and_hash_function {

// Universal hash function implementation.
template <typename T, bool HAS_MEMBER_HASH_FUNCTION, bool IS_ENUM>
struct GenericHashFunctionImpl;

template <typename T>
constexpr bool HasHashMethod(char) {
  return false;
}

template <typename T>
constexpr auto HasHashMethod(int) -> decltype(std::declval<const T>().Hash(), bool()) {
  return true;
}

template <typename T>
struct GenericHashFunctionSelector {
  typedef custom_comparator_and_hash_function::GenericHashFunctionImpl<T, HasHashMethod<T>(0), std::is_enum_v<T>> type;
};

}  // namespace custom_comparator_and_hash_function

//  `current::GenericHashFunction<T>`.
template <typename T>
using GenericHashFunction = typename custom_comparator_and_hash_function::GenericHashFunctionSelector<T>::type;

namespace custom_comparator_and_hash_function {

template <typename T>
struct GenericHashFunctionImpl<T, false, false> : std::hash<T> {};

template <typename TF, typename TS>
struct GenericHashFunctionImpl<std::pair<TF, TS>, false, false> {
  std::size_t operator()(const std::pair<TF, TS>& p) const {
    std::size_t seed = 0u;
    HashCombine<TF, GenericHashFunction<TF>>(seed, p.first);
    HashCombine<TS, GenericHashFunction<TS>>(seed, p.second);
    return seed;
  }
};

template <typename R, typename P>
struct GenericHashFunctionImpl<std::chrono::duration<R, P>, false, false> {
  std::size_t operator()(std::chrono::duration<R, P> x) const {
    return std::hash<int64_t>()(std::chrono::duration_cast<std::chrono::microseconds>(x).count());
  }
};

template <typename T>
struct GenericHashFunctionImpl<T, false, true> {
  std::size_t operator()(T x) const { return static_cast<size_t>(x); }
};

template <typename T>
struct GenericHashFunctionImpl<T, true, false> {
  std::size_t operator()(const T& x) const { return x.Hash(); }
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

template <typename T>
using CurrentComparator = custom_comparator_and_hash_function::CurrentComparatorImpl<T, std::is_enum_v<T>>;

}  // namespace current

#endif  // BRICKS_UTIL_COMPARATOR_H
