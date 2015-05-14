/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef SHERLOCK_YODA_API_MATRIX_METAPROGRAMMING_H
#define SHERLOCK_YODA_API_MATRIX_METAPROGRAMMING_H

#include <utility>

#include "../../metaprogramming.h"
#include "../../../../Bricks/template/pod.h"
#include "../../../../Bricks/template/rmref.h"

namespace yoda {

namespace sfinae {
// Matrix row and column type extractors, getters and setters.
// Support two access methods:
// - `.row / .col` data members,
// - `.row() / set_row() / .col() / .set_col()` methods.
template <typename T_ENTRY>
constexpr bool HasRowFunction(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasRowFunction(int) -> decltype(std::declval<T_ENTRY>().row(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_ROW_FUNCTION>
struct ROW_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct ROW_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().row) T_ROW;
  static typename bricks::copy_free<T_ROW> GetRow(const T_ENTRY& entry) { return entry.row; }
  static void SetRow(T_ENTRY& entry, T_ROW row) { entry.row = row; }
};

template <typename T_ENTRY>
struct ROW_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().row()) T_ROW;
  static typename bricks::copy_free<T_ROW> GetRow(const T_ENTRY& entry) { return entry.row(); }
  static void SetRow(T_ENTRY& entry, T_ROW row) { entry.set_row(row); }
};

template <typename T_ENTRY>
using ROW_ACCESSOR = ROW_ACCESSOR_IMPL<T_ENTRY, HasRowFunction<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename ROW_ACCESSOR<T_ENTRY>::T_ROW GetRow(const T_ENTRY& entry) {
  return ROW_ACCESSOR<T_ENTRY>::GetRow(entry);
}

template <typename T_ENTRY>
using ENTRY_ROW_TYPE = bricks::rmconstref<typename ROW_ACCESSOR<T_ENTRY>::T_ROW>;

template <typename T_ENTRY>
void SetRow(T_ENTRY& entry, ENTRY_ROW_TYPE<T_ENTRY> row) {
  ROW_ACCESSOR<T_ENTRY>::SetRow(entry, row);
}

template <typename T_ENTRY>
constexpr bool HasColFunction(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasColFunction(int) -> decltype(std::declval<T_ENTRY>().key(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_COL_FUNCTION>
struct COL_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct COL_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().col) T_COL;
  static typename bricks::copy_free<T_COL> GetCol(const T_ENTRY& entry) { return entry.col; }
  static void SetCol(T_ENTRY& entry, T_COL col) { entry.col = col; }
};

template <typename T_ENTRY>
struct COL_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().col()) T_COL;
  static typename bricks::copy_free<T_COL> GetCol(const T_ENTRY& entry) { return entry.col(); }
  static void SetCol(T_ENTRY& entry, T_COL col) { entry.set_col(col); }
};

template <typename T_ENTRY>
using COL_ACCESSOR = COL_ACCESSOR_IMPL<T_ENTRY, HasColFunction<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename COL_ACCESSOR<T_ENTRY>::T_COL GetCol(const T_ENTRY& entry) {
  return COL_ACCESSOR<T_ENTRY>::GetCol(entry);
}

template <typename T_ENTRY>
using ENTRY_COL_TYPE = bricks::rmconstref<typename COL_ACCESSOR<T_ENTRY>::T_COL>;

template <typename T_ENTRY>
void SetCol(T_ENTRY& entry, ENTRY_COL_TYPE<T_ENTRY> key) {
  COL_ACCESSOR<T_ENTRY>::SetCol(entry, key);
}

}  // namespace sfinae
}  // namespace yoda

#endif  // SHERLOCK_YODA_API_MATRIX_METAPROGRAMMING_H
