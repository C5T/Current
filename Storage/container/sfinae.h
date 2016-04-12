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

#include "../../Bricks/template/metaprogramming.h"

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
  typedef decltype(std::declval<ENTRY>().key) key_t;
  static CF<key_t> GetKey(const ENTRY& entry) { return entry.key; }
  static void SetKey(ENTRY& entry, key_t key) { entry.key = key; }
};

template <typename ENTRY>
struct impl_key_accessor_t<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().key()) key_t;
  // Can not return a reference to a temporary.
  static const key_t GetKey(const ENTRY& entry) { return entry.key(); }
  static void SetKey(ENTRY& entry, CF<key_t> key) { entry.set_key(key); }
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
typename key_accessor_t<ENTRY>::key_t GetKey(const ENTRY& entry) {
  return key_accessor_t<ENTRY>::GetKey(entry);
}

template <typename ENTRY>
using ENTRY_KEY_TYPE = current::decay<typename key_accessor_t<ENTRY>::key_t>;

template <typename ENTRY>
void SetKey(ENTRY& entry, CF<ENTRY_KEY_TYPE<ENTRY>> key) {
  key_accessor_t<ENTRY>::SetKey(entry, key);
}

// Matrix row and column type extractors, getters and setters.
// Support two access methods:
// - `.row / .col` data members,
// - `.row() / set_row() / .col() / .set_col()` methods.
template <typename ENTRY>
constexpr bool HasRowFunction(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasRowFunction(int) -> decltype(std::declval<const ENTRY>().row(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_ROW_FUNCTION>
struct impl_row_accessor_t {};

template <typename ENTRY>
struct impl_row_accessor_t<ENTRY, false> {
  typedef decltype(std::declval<ENTRY>().row) row_t;
  static CF<row_t> GetRow(const ENTRY& entry) { return entry.row; }
  static void SetRow(ENTRY& entry, CF<row_t> row) { entry.row = row; }
};

template <typename ENTRY>
struct impl_row_accessor_t<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().row()) row_t;
  // Can not return a reference to a temporary.
  static const row_t GetRow(const ENTRY& entry) { return entry.row(); }
  static void SetRow(ENTRY& entry, CF<row_t> row) { entry.set_row(row); }
};

template <typename ROW, typename COL>
struct impl_row_accessor_t<std::pair<ROW, COL>, false> {
  typedef ROW row_t;
  static CF<row_t> GetRow(const std::pair<ROW, COL>& entry) { return entry.first; }
  static void SetRow(std::pair<ROW, COL>& entry, CF<row_t> row) { entry.first = row; }
};

template <typename ENTRY>
using row_accessor_t = impl_row_accessor_t<ENTRY, HasRowFunction<ENTRY>(0)>;

template <typename ENTRY>
typename row_accessor_t<ENTRY>::row_t GetRow(const ENTRY& entry) {
  return row_accessor_t<ENTRY>::GetRow(entry);
}

template <typename ENTRY>
using entry_row_t = current::decay<typename row_accessor_t<ENTRY>::row_t>;

template <typename ENTRY>
void SetRow(ENTRY& entry, CF<entry_row_t<ENTRY>> row) {
  row_accessor_t<ENTRY>::SetRow(entry, row);
}

template <typename ENTRY>
constexpr bool HasColFunction(char) {
  return false;
}

template <typename ENTRY>
constexpr auto HasColFunction(int) -> decltype(std::declval<const ENTRY>().col(), bool()) {
  return true;
}

template <typename ENTRY, bool HAS_COL_FUNCTION>
struct impl_col_accessor_t {};

template <typename ENTRY>
struct impl_col_accessor_t<ENTRY, false> {
  typedef decltype(std::declval<ENTRY>().col) col_t;
  static CF<col_t> GetCol(const ENTRY& entry) { return entry.col; }
  static void SetCol(ENTRY& entry, col_t col) { entry.col = col; }
};

template <typename ENTRY>
struct impl_col_accessor_t<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().col()) col_t;
  // Can not return a reference to a temporary.
  static const col_t GetCol(const ENTRY& entry) { return entry.col(); }
  static void SetCol(ENTRY& entry, CF<col_t> col) { entry.set_col(col); }
};

template <typename ROW, typename COL>
struct impl_col_accessor_t<std::pair<ROW, COL>, false> {
  typedef COL col_t;
  static CF<col_t> GetCol(const std::pair<COL, COL>& entry) { return entry.second; }
  static void SetCol(std::pair<COL, COL>& entry, CF<col_t> col) { entry.second = col; }
};

template <typename ENTRY>
using col_accessor_t = impl_col_accessor_t<ENTRY, HasColFunction<ENTRY>(0)>;

template <typename ENTRY>
typename col_accessor_t<ENTRY>::col_t GetCol(const ENTRY& entry) {
  return col_accessor_t<ENTRY>::GetCol(entry);
}

template <typename ENTRY>
using entry_col_t = current::decay<typename col_accessor_t<ENTRY>::col_t>;

template <typename ENTRY>
void SetCol(ENTRY& entry, CF<entry_col_t<ENTRY>> col) {
  col_accessor_t<ENTRY>::SetCol(entry, col);
}

}  // namespace sfinae
}  // namespace storage
}  // namespace current

#endif  // CURRENT_STORAGE_CONTAINER_SFINAE_H
