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

#ifndef SHERLOCK_YODA_TYPES_H
#define SHERLOCK_YODA_TYPES_H

#include <map>
#include <unordered_map>
#include <type_traits>

namespace yoda {

// Helper structures that the user can derive their entries from
// to signal Yoda to behave in a non-default way.

struct AllowNonThrowingGet {
  // Inheriting from this base class, or simply defining the below constant manually,
  // will result in `Get()`-s for non-existing keys to be non-throwing.
  // NOTE: This behavior requires the user class to derive from `Nullable` as well, see below.
  constexpr static bool allow_nonthrowing_get = true;
};

struct AllowOverwriteOnAdd {
  // Inheriting from this base class, or simply defining the below constant manually,
  // will result in `Add()`-s for already existing keys to silently overwrite previous values.
  constexpr static bool allow_overwrite_on_add = true;
};

// Helper structures that the user can derive their entries from
// to implement particular Yoda functionality.

// By deriving from `Nullable` (and adding `using Nullable::Nullable`),
// the user indicates that their entry type supports creation of a non-existing instance.
// This is a requirement for a) non-throwing `Get()`, and b) for `Delete()` part of the API.
// TODO(dkorolev): Add a compile-time check that `Nullable` user types allow constructing themselves as null-s.
enum NullEntryTypeHelper { NullEntry };
struct Nullable {
  const bool exists;
  Nullable() : exists(true) {}
  Nullable(NullEntryTypeHelper) : exists(false) {}
};

// By deriving from `Deletable`, the user commits to serializing the `Nullable::exists` field,
// thus enabling delete-friendly storage.
// The user should derive from both `Nullable` and `Deletable` for full `Delete()` support via Yoda.
// TODO(dkorolev): Add the runtime check that certain type does indeed serialize the `exists` field and test it.
struct Deletable {};

// TODO(dkorolev): Let's move this to Bricks once we merge repositories?
// Associative container type selector. Attempts to use:
// 1) std::unordered_map<T_KEY, T_ENTRY, wrapper for `T_KEY::Hash()`>
// 2) std::unordered_map<T_KEY, T_ENTRY [, std::hash<T_KEY>]>
// 3) std::map<T_KEY, T_ENTRY>
// in the above order.

template <typename T_KEY>
constexpr bool HasHashFunction(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasHashFunction(int) -> decltype(std::declval<T_KEY>().Hash(), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasStdHash(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasStdHash(int) -> decltype(std::hash<T_KEY>(), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasOperatorEquals(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasOperatorEquals(int)
    -> decltype(static_cast<bool>(std::declval<T_KEY>() == std::declval<T_KEY>()), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasOperatorLess(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasOperatorLess(int)
    -> decltype(static_cast<bool>(std::declval<T_KEY>() < std::declval<T_KEY>()), bool()) {
  return true;
}

template <typename T_KEY, typename T_ENTRY, bool HAS_CUSTOM_HASH_FUNCTION, bool DEFINES_STD_HASH>
struct T_MAP_TYPE_SELECTOR {};

// `T_KEY::Hash()` and `T_KEY::operator==()` are defined, use std::unordered_map<> with user-defined hash
// function.
template <typename T_KEY, typename T_ENTRY, bool DEFINES_STD_HASH>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, true, DEFINES_STD_HASH> {
  static_assert(HasOperatorEquals<T_KEY>(0), "The key type defines `Hash()`, but not `operator==()`.");
  struct HashFunction {
    size_t operator()(const T_KEY& key) const { return static_cast<size_t>(key.Hash()); }
  };
  typedef std::unordered_map<T_KEY, T_ENTRY, HashFunction> type;
};

// `T_KEY::Hash()` is not defined, but `std::hash<T_KEY>` and `T_KEY::operator==()` are, use
// std::unordered_map<>.
template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, false, true> {
  static_assert(HasOperatorEquals<T_KEY>(0),
                "The key type supports `std::hash<T_KEY>`, but not `operator==()`.");
  typedef std::unordered_map<T_KEY, T_ENTRY> type;
};

// Neither `T_KEY::Hash()` nor `std::hash<T_KEY>` are defined, use std::map<>.
template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, false, false> {
  static_assert(HasOperatorLess<T_KEY>(0), "The key type defines neither `Hash()` nor `operator<()`.");
  typedef std::map<T_KEY, T_ENTRY> type;
};

template <typename T_KEY, typename T_ENTRY>
using T_MAP_TYPE =
    typename T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, HasHashFunction<T_KEY>(0), HasStdHash<T_KEY>(0)>::type;

}  // namespace yoda

#endif  // SHERLOCK_YODA_TYPES_H
