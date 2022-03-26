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

#ifndef CURRENT_STORAGE_CONTAINER_SFINAE_H
#define CURRENT_STORAGE_CONTAINER_SFINAE_H

#include <utility>

#ifdef CURRENT_STORAGE_PATCH_SUPPORT
#include "../../typesystem/struct.h"
#endif  // CURRENT_STORAGE_PATCH_SUPPORT

#include "../../bricks/template/metaprogramming.h"

namespace current {
namespace storage {
namespace sfinae {

template <typename T>
using CF = current::copy_free<T>;

// Entry key type extractor, getter and setter.
// Supports both `.key` data member and `.key() / .set_key()` methods.
template <typename ENTRY>
constexpr bool HasKeyMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasKeyMethod(int) -> decltype(std::declval<const ENTRY>().key(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_KEY_FUNCTION>
struct impl_key_accessor_t {};

template <typename ENTRY>
struct impl_key_accessor_t<ENTRY, false> {
  using key_t = decltype(ENTRY::key);
  using cf_key_t = CF<key_t>;

  static cf_key_t GetKey(const ENTRY& entry) { return entry.key; }
  static void SetKey(ENTRY& entry, cf_key_t key) { entry.key = key; }
};

template <typename ENTRY>
struct impl_key_accessor_t<ENTRY, true> {
  // CF return type by design.
  using cf_key_t = decltype(std::declval<ENTRY>().key());
  using key_t = current::decay_t<cf_key_t>;
  static_assert(std::is_same_v<cf_key_t, CF<key_t>>, "");

  static cf_key_t GetKey(const ENTRY& entry) { return entry.key(); }
  static void SetKey(ENTRY& entry, cf_key_t key) { entry.set_key(key); }
};

#ifndef CURRENT_WINDOWS
template <typename ENTRY>
using key_accessor_t = impl_key_accessor_t<ENTRY, HasKeyMethod<ENTRY>(0)>;
#else
// Visual C++ [Enterprise 2015, 00322-8000-00000-AA343] is not friendly with a `constexpr` "call" from "using".
// Work around it by introducing another `struct`. -- D.K.
template <typename ENTRY>
struct vs_impl_key_accessor_t {
  typedef impl_key_accessor_t<ENTRY, HasKeyMethod<ENTRY>(0)> type;
};
template <typename ENTRY>
using key_accessor_t = typename vs_impl_key_accessor_t<ENTRY>::type;
#endif  // CURRENT_WINDOWS

template <typename ENTRY>
using entry_key_t = typename key_accessor_t<ENTRY>::key_t;

template <typename ENTRY>
typename key_accessor_t<ENTRY>::cf_key_t GetKey(const ENTRY& entry) {
  return key_accessor_t<ENTRY>::GetKey(entry);
}

template <typename ENTRY>
void SetKey(ENTRY& entry, typename key_accessor_t<ENTRY>::cf_key_t key) {
  key_accessor_t<ENTRY>::SetKey(entry, key);
}

// Matrix row and column type extractors, getters and setters.
// Support two access methods:
// - `.row / .col` data members,
// - `.row() / set_row() / .col() / .set_col()` methods.
template <typename ENTRY>
constexpr bool HasRowMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasRowMethod(int) -> decltype(std::declval<const ENTRY>().row(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_ROW_FUNCTION>
struct impl_row_accessor_t {};

template <typename ENTRY>
struct impl_row_accessor_t<ENTRY, false> {
  using row_t = decltype(ENTRY::row);
  using cf_row_t = CF<row_t>;

  static cf_row_t GetRow(const ENTRY& entry) { return entry.row; }
  static void SetRow(ENTRY& entry, cf_row_t row) { entry.row = row; }
};

template <typename ENTRY>
struct impl_row_accessor_t<ENTRY, true> {
  using cf_row_t = decltype(std::declval<ENTRY>().row());
  using row_t = current::decay_t<cf_row_t>;
  static_assert(std::is_same_v<cf_row_t, CF<row_t>>, "");

  static cf_row_t GetRow(const ENTRY& entry) { return entry.row(); }
  static void SetRow(ENTRY& entry, cf_row_t row) { entry.set_row(row); }
};

template <typename ROW, typename COL>
struct impl_row_accessor_t<std::pair<ROW, COL>, false> {
  static_assert(std::is_same_v<ROW, current::decay_t<ROW>>, "");
  static_assert(std::is_same_v<COL, current::decay_t<COL>>, "");
  using pair_t = std::pair<ROW, COL>;
  using row_t = ROW;
  using cf_row_t = CF<ROW>;

  static cf_row_t GetRow(const pair_t& entry) { return entry.first; }
  static void SetRow(pair_t& entry, cf_row_t row) { entry.first = row; }
};

#ifndef CURRENT_WINDOWS
template <typename ENTRY>
using row_accessor_t = impl_row_accessor_t<ENTRY, HasRowMethod<ENTRY>(0)>;
#else
// Visual C++ [Enterprise 2015, 00322-8000-00000-AA343] is not friendly with a `constexpr` "call" from "using".
// Work around it by introducing another `struct`. -- D.K.
template <typename ENTRY>
struct vs_impl_row_accessor_t {
  typedef impl_row_accessor_t<ENTRY, HasRowMethod<ENTRY>(0)> type;
};
template <typename ENTRY>
using row_accessor_t = typename vs_impl_row_accessor_t<ENTRY>::type;
#endif  // CURRENT_WINDOWS

template <typename ENTRY>
typename row_accessor_t<ENTRY>::cf_row_t GetRow(const ENTRY& entry) {
  return row_accessor_t<ENTRY>::GetRow(entry);
}

template <typename ENTRY>
using entry_row_t = typename row_accessor_t<ENTRY>::row_t;

template <typename ENTRY>
void SetRow(ENTRY& entry, typename row_accessor_t<ENTRY>::cf_row_t row) {
  row_accessor_t<ENTRY>::SetRow(entry, row);
}

template <typename ENTRY>
constexpr bool HasColMethod(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasColMethod(int) -> decltype(std::declval<const ENTRY>().col(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_COL_FUNCTION>
struct impl_col_accessor_t {};

template <typename ENTRY>
struct impl_col_accessor_t<ENTRY, false> {
  using col_t = decltype(ENTRY::col);
  using cf_col_t = CF<col_t>;

  static cf_col_t GetCol(const ENTRY& entry) { return entry.col; }
  static void SetCol(ENTRY& entry, cf_col_t col) { entry.col = col; }
};

template <typename ENTRY>
struct impl_col_accessor_t<ENTRY, true> {
  using cf_col_t = decltype(std::declval<ENTRY>().col());
  using col_t = current::decay_t<cf_col_t>;
  static_assert(std::is_same_v<cf_col_t, CF<col_t>>, "");

  static cf_col_t GetCol(const ENTRY& entry) { return entry.col(); }
  static void SetCol(ENTRY& entry, cf_col_t col) { entry.set_col(col); }
};

template <typename ROW, typename COL>
struct impl_col_accessor_t<std::pair<ROW, COL>, false> {
  static_assert(std::is_same_v<ROW, current::decay_t<ROW>>, "");
  static_assert(std::is_same_v<COL, current::decay_t<COL>>, "");
  using pair_t = std::pair<ROW, COL>;
  using col_t = COL;
  using cf_col_t = CF<COL>;

  static cf_col_t GetCol(const pair_t& entry) { return entry.second; }
  static void SetCol(pair_t& entry, cf_col_t col) { entry.second = col; }
};

#ifndef CURRENT_WINDOWS
template <typename ENTRY>
using col_accessor_t = impl_col_accessor_t<ENTRY, HasColMethod<ENTRY>(0)>;
#else
// Visual C++ [Enterprise 2015, 00322-8000-00000-AA343] is not friendly with a `constexpr` "call" from "using".
// Work around it by introducing another `struct`. -- D.K.
template <typename ENTRY>
struct vs_impl_col_accessor_t {
  typedef impl_col_accessor_t<ENTRY, HasColMethod<ENTRY>(0)> type;
};
template <typename ENTRY>
using col_accessor_t = typename vs_impl_col_accessor_t<ENTRY>::type;
#endif  // CURRENT_WINDOWS

template <typename ENTRY>
typename col_accessor_t<ENTRY>::cf_col_t GetCol(const ENTRY& entry) {
  return col_accessor_t<ENTRY>::GetCol(entry);
}

template <typename ENTRY>
using entry_col_t = typename col_accessor_t<ENTRY>::col_t;

template <typename ENTRY>
void SetCol(ENTRY& entry, typename col_accessor_t<ENTRY>::cf_col_t col) {
  col_accessor_t<ENTRY>::SetCol(entry, col);
}

#ifdef CURRENT_STORAGE_PATCH_SUPPORT

CURRENT_STRUCT(DummyPatchObjectForNonPatchableEntries){};

template <bool, class>
struct patch_object_t_accessor {
  using patch_object_t = DummyPatchObjectForNonPatchableEntries;
};

template <class ENTRY>
struct patch_object_t_accessor<true, ENTRY> {
  using patch_object_t = typename ENTRY::patch_object_t;
};

template <typename ENTRY>
using entry_patch_object_t = typename patch_object_t_accessor<HasPatch<ENTRY>(), ENTRY>::patch_object_t;

#endif  // CURRENT_STORAGE_PATCH_SUPPORT

}  // namespace sfinae
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_CONTAINER_SFINAE_H
