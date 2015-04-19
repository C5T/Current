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

// Entry key type extractor, getter and setter.
// Supports both `.key` data member and `.key() / .set_key()` methods.
template <typename T_ENTRY>
constexpr bool HasKeyFunction(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasKeyFunction(int) -> decltype(std::declval<T_ENTRY>().key(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_KEY_FUNCTION>
struct KEY_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct KEY_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().key) T_KEY;
  static T_KEY GetKey(const T_ENTRY& entry) { return entry.key; }
  static void SetKey(T_ENTRY& entry, T_KEY key) { entry.key = key; }
};

template <typename T_ENTRY>
struct KEY_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().key()) T_KEY;
  static T_KEY GetKey(const T_ENTRY& entry) { return entry.key(); }
  static void SetKey(T_ENTRY& entry, T_KEY key) { entry.set_key(key); }
};

template <typename T_ENTRY>
using KEY_ACCESSOR = KEY_ACCESSOR_IMPL<T_ENTRY, HasKeyFunction<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename KEY_ACCESSOR<T_ENTRY>::T_KEY GetKey(const T_ENTRY& entry) {
  return KEY_ACCESSOR<T_ENTRY>::GetKey(entry);
}

template <typename T_ENTRY>
using ENTRY_KEY_TYPE =
    typename std::remove_cv<typename std::remove_reference<typename KEY_ACCESSOR<T_ENTRY>::T_KEY>::type>::type;

template <typename T_ENTRY>
void SetKey(T_ENTRY& entry, ENTRY_KEY_TYPE<T_ENTRY> key) {
  KEY_ACCESSOR<T_ENTRY>::SetKey(entry, key);
}

// Associative container type selector.
// Tries unordered_map<T_KEY, T_ENTRY, {T_KEY::Hash}>, falls back to map<T_KEY, T_ENTRY> if unavailable.

template <typename T>
constexpr bool HasHashFunction(char) {
  return false;
}

template <typename T>
constexpr auto HasHashFunction(int) -> decltype(std::declval<T>().Hash(), bool()) {
  return true;
}

template <typename T_KEY, typename T_ENTRY, bool CAN_CUSTOM_HASH_MAP>
struct T_MAP_TYPE_SELECTOR {};

template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, true> {
  struct HashFunction {
    size_t operator()(const T_KEY& key) const { return static_cast<size_t>(key.Hash()); }
  };
  typedef std::unordered_map<T_KEY, T_ENTRY, HashFunction> type;
};

template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, false> {
  typedef std::map<T_KEY, T_ENTRY> type;
};

template <typename T_KEY, typename T_ENTRY>
using T_MAP_TYPE = typename T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, HasHashFunction<T_KEY>(0)>::type;

// By deriving from `Nullable` (and adding `using Nullable::Nullable`),
// the user indicates that their entry type supports creation of a non-existing instance.
// This is a requirement for a) non-throwing `Get()`, and b) for `Delete()` part of the API.
enum NullEntryTypeHelper { NullEntry };
struct Nullable {
  const bool exists;
  Nullable() : exists(true) {}
  Nullable(NullEntryTypeHelper) : exists(false) {}
};

// By deriving from `Deletable`, the user commits to serializing the `Nullable::exists` field,
// thus enabling delete-friendly storage.
// The user should derive from both `Nullable` and `Deletable` for full `Delete()` support via Yoda.
struct Deletable {};

}  // namespace yoda

#endif  // SHERLOCK_YODA_TYPES_H
