/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef SHERLOCK_YODA_SFINAE_H
#define SHERLOCK_YODA_SFINAE_H

#include <functional>
#include <string>
#include <map>
#include <unordered_map>
#include <utility>

namespace yoda {
namespace sfinae {

// TODO(dkorolev): Let's move this to Bricks once we merge repositories?
// Associative container type selector. Attempts to use:
// 1) std::unordered_map<KEY, ENTRY, wrapper for `KEY::Hash()`>
// 2) std::unordered_map<KEY, ENTRY [, std::hash<KEY>]>
// 3) std::map<KEY, ENTRY>
// in the above order.

template <typename KEY>
constexpr bool HasHashFunction(char) {
  return false;
}

template <typename KEY>
constexpr auto HasHashFunction(int) -> decltype(std::declval<KEY>().Hash(), bool()) {
  return true;
}

template <typename KEY>
constexpr bool HasStdHash(char) {
  return false;
}

template <typename KEY>
constexpr auto HasStdHash(int) -> decltype(std::hash<KEY>(), bool()) {
  return true;
}

template <typename KEY>
constexpr bool EnumHasStdHash(char) {
  return false;
}

template <typename KEY>
constexpr auto EnumHasStdHash(int) -> decltype(std::hash<typename std::underlying_type<KEY>::type>(), bool()) {
  return true;
}

template <typename KEY>
constexpr bool HasOperatorEquals(char) {
  return false;
}

template <typename KEY>
constexpr auto HasOperatorEquals(int)
    -> decltype(static_cast<bool>(std::declval<KEY>() == std::declval<KEY>()), bool()) {
  return true;
}

template <typename KEY>
constexpr bool HasOperatorLess(char) {
  return false;
}

template <typename KEY>
constexpr auto HasOperatorLess(int)
    -> decltype(static_cast<bool>(std::declval<KEY>() < std::declval<KEY>()), bool()) {
  return true;
}

static_assert(HasOperatorLess<std::string>(0), "");

template <typename KEY, bool IS_ENUM = false>
struct GenericHasStdHash {
  static constexpr bool value() { return HasStdHash<KEY>(0); }
};

template <typename KEY>
struct GenericHasStdHash<KEY, true> {
  static constexpr bool value() { return EnumHasStdHash<KEY>(0); }
};

template <typename KEY>
constexpr bool HasStdOrCustomHash() {
  return GenericHasStdHash<KEY, std::is_enum<KEY>::value>::value() || HasHashFunction<KEY>(0);
};

template <typename KEY>
constexpr bool FitsAsKeyForUnorderedMap() {
  return HasStdOrCustomHash<KEY>() && HasOperatorEquals<KEY>(0);
};

template <typename KEY, bool HAS_CUSTOM_HASH_FUNCTION, bool IS_ENUM>
struct HASH_SELECTOR;

template <typename KEY>
struct HASH_SELECTOR<KEY, true, false> {
  struct HashFunction {
    size_t operator()(const KEY& key) const { return static_cast<size_t>(key.Hash()); }
  };
  typedef HashFunction type;
};

template <typename KEY>
struct HASH_SELECTOR<KEY, false, true> {
  struct HashFunction {
    typedef typename std::underlying_type<KEY>::type UT;
    size_t operator()(const KEY& key) const { return std::hash<UT>()(static_cast<UT>(key)); }
  };
  typedef HashFunction type;
};

template <typename KEY>
struct HASH_SELECTOR<KEY, false, false> {
  typedef std::hash<KEY> type;
};

template <typename KEY>
using HASH_FUNCTION = typename HASH_SELECTOR<KEY, HasHashFunction<KEY>(0), std::is_enum<KEY>::value>::type;

template <typename KEY, typename ENTRY, bool HAS_CUSTOM_HASH_FUNCTION, bool DEFINES_STD_HASH>
struct MAP_TYPE_SELECTOR {};

// If `KEY::Hash()` and `operator==()` are defined, use `std::unordered_map<>` with `KEY::Hash()`.
template <typename KEY, typename ENTRY, bool DEFINES_STD_HASH>
struct MAP_TYPE_SELECTOR<KEY, ENTRY, true, DEFINES_STD_HASH> {
  static_assert(HasOperatorEquals<KEY>(0), "The key type defines `Hash()`, but not `operator==()`.");
  typedef std::unordered_map<KEY, ENTRY, HASH_FUNCTION<KEY>> type;
};

// If `KEY::Hash()` is not defined, but both `std::hash<>` (for either KEY or its underlying type)
// and `KEY::operator==()` are, use `std::unordered_map<>` with the default `std::hash<>`.
template <typename KEY, typename ENTRY>
struct MAP_TYPE_SELECTOR<KEY, ENTRY, false, true> {
  static_assert(HasOperatorEquals<KEY>(0), "The key type supports `std::hash<KEY>`, but not `operator==()`.");
  typedef std::unordered_map<KEY, ENTRY, HASH_FUNCTION<KEY>> type;
};

// If neither `KEY::Hash()` nor `std::hash<>` (for either KEY or its underlying type) are defined,
// use `std::map<>`.
template <typename KEY, typename ENTRY>
struct MAP_TYPE_SELECTOR<KEY, ENTRY, false, false> {
  static_assert(HasOperatorLess<KEY>(0), "The key type defines neither `Hash()` nor `operator<()`.");
  typedef std::map<KEY, ENTRY> type;
};

// `MAP_TYPE` is a reserved keyword in Ubuntu. What a shame. -- D.K.
// L47 of `/usr/include/x86_64-linux-gnu/bits/mman.h`: `#define MAP_TYPE 0x0f  // Mask for type of mapping.`
template <typename KEY, typename ENTRY>
using MAP_CONTAINER_TYPE =
    typename MAP_TYPE_SELECTOR<KEY,
                               ENTRY,
                               HasHashFunction<KEY>(0),
                               GenericHasStdHash<KEY, std::is_enum<KEY>::value>::value()>::type;

// Check that SFINAE does the magic right.
namespace test {
static_assert(std::is_same<std::unordered_map<int, int>, MAP_CONTAINER_TYPE<int, int>>::value, "");
static_assert(std::is_same<std::unordered_map<std::string, int>, MAP_CONTAINER_TYPE<std::string, int>>::value,
              "");

enum class A : int;
static_assert(std::is_same<std::unordered_map<A, int, typename HASH_SELECTOR<A, false, true>::type>,
                           MAP_CONTAINER_TYPE<A, int>>::value,
              "");

struct B {
  bool operator<(const B&) const { return true; }
};
static_assert(std::is_same<std::map<B, int>, MAP_CONTAINER_TYPE<B, int>>::value, "");
}  // namespace test

}  // namespace sfinae
}  // namespace yoda

#endif  // SHERLOCK_YODA_SFINAE_H
