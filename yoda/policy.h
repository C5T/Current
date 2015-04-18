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

#ifndef SHERLOCK_YODA_POLICY_H
#define SHERLOCK_YODA_POLICY_H

#include <future>
#include <utility>

#include "types.h"

namespace yoda {

// Non-throwing get policy and its default.
template <typename ENTRY>
constexpr auto nonthrowing_get_value_or_default(int)
    -> decltype(std::declval<ENTRY>().allow_nonthrowing_get, bool()) {
  return ENTRY::allow_nonthrowing_get;
};

template <typename ENTRY>
constexpr bool nonthrowing_get_value_or_default(...) {
  // Default: Throw if the key is not present in the storage.
  return false;
}

// Silent overwrite on add policy and its default.
template <typename ENTRY>
constexpr auto overwrite_on_add_value_or_default(int)
    -> decltype(std::declval<ENTRY>().allow_overwrite_on_add, bool()) {
  return ENTRY::allow_overwrite_on_add;
}

template <typename ENTRY>
constexpr bool overwrite_on_add_value_or_default(...) {
  // Default: Throw, don't overwrite on `Add()` when the key already exists.
  return false;
}

// Default set of policies for the instance of Yoda.
template <typename ENTRY>
struct DefaultPolicy {
  constexpr static bool allow_nonthrowing_get = nonthrowing_get_value_or_default<ENTRY>(0);
  constexpr static bool allow_overwrite_on_add = overwrite_on_add_value_or_default<ENTRY>(0);
  static_assert(!allow_nonthrowing_get || std::is_base_of<Nullable, ENTRY>::value,
                "To support non-throwing `Get()`, make the entry types derive from `yoda::Nullable`.");
};

// The implementation for the logic of `allow_nonthrowing_get`.
template <typename T_KEY, typename T_ENTRY, typename T_KEY_NOT_FOUND_EXCEPTION, bool>
struct SetPromiseToNullEntryOrThrow {};

template <typename T_KEY, typename T_ENTRY, typename T_KEY_NOT_FOUND_EXCEPTION>
struct SetPromiseToNullEntryOrThrow<T_KEY, T_ENTRY, T_KEY_NOT_FOUND_EXCEPTION, false> {
  static void DoIt(const T_KEY& key, std::promise<T_ENTRY>& pr) {
    pr.set_exception(std::make_exception_ptr(T_KEY_NOT_FOUND_EXCEPTION(key)));
  }
};

template <typename T_KEY, typename T_ENTRY, typename UNUSED_T_KEY_NOT_FOUND_EXCEPTION>
struct SetPromiseToNullEntryOrThrow<T_KEY, T_ENTRY, UNUSED_T_KEY_NOT_FOUND_EXCEPTION, true> {
  static void DoIt(const T_KEY& key, std::promise<T_ENTRY>& pr) {
    T_ENTRY null_entry(NullEntry);
    null_entry.set_key(key);
    pr.set_value(null_entry);
  }
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_POLICY_H
