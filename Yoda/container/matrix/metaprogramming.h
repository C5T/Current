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

#ifndef SHERLOCK_YODA_CONTAINER_MATRIX_METAPROGRAMMING_H
#define SHERLOCK_YODA_CONTAINER_MATRIX_METAPROGRAMMING_H

#include <tuple>
#include <utility>

#include "../../types.h"
#include "../../metaprogramming.h"
#include "../../sfinae.h"
#include "../../../Bricks/template/pod.h"
#include "../../../Bricks/template/decay.h"

namespace yoda {

namespace sfinae {

template <typename T>
using CF = current::copy_free<T>;

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
struct ROW_ACCESSOR_IMPL {};

template <typename ENTRY>
struct ROW_ACCESSOR_IMPL<ENTRY, false> {
  typedef decltype(std::declval<ENTRY>().row) T_ROW;
  static CF<T_ROW> GetRow(const ENTRY& entry) { return entry.row; }
  static void SetRow(ENTRY& entry, CF<T_ROW> row) { entry.row = row; }
};

template <typename ENTRY>
struct ROW_ACCESSOR_IMPL<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().row()) T_ROW;
  // Can not return a reference to a temporary.
  static const T_ROW GetRow(const ENTRY& entry) { return entry.row(); }
  static void SetRow(ENTRY& entry, CF<T_ROW> row) { entry.set_row(row); }
};

template <typename ENTRY>
using ROW_ACCESSOR = ROW_ACCESSOR_IMPL<ENTRY, HasRowFunction<ENTRY>(0)>;

template <typename ENTRY>
typename ROW_ACCESSOR<ENTRY>::T_ROW GetRow(const ENTRY& entry) {
  return ROW_ACCESSOR<ENTRY>::GetRow(entry);
}

template <typename ENTRY>
using ENTRY_ROW_TYPE = current::decay<typename ROW_ACCESSOR<ENTRY>::T_ROW>;

template <typename ENTRY>
void SetRow(ENTRY& entry, CF<ENTRY_ROW_TYPE<ENTRY>> row) {
  ROW_ACCESSOR<ENTRY>::SetRow(entry, row);
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
struct COL_ACCESSOR_IMPL {};

template <typename ENTRY>
struct COL_ACCESSOR_IMPL<ENTRY, false> {
  typedef decltype(std::declval<ENTRY>().col) T_COL;
  static CF<T_COL> GetCol(const ENTRY& entry) { return entry.col; }
  static void SetCol(ENTRY& entry, T_COL col) { entry.col = col; }
};

template <typename ENTRY>
struct COL_ACCESSOR_IMPL<ENTRY, true> {
  typedef decltype(std::declval<ENTRY>().col()) T_COL;
  // Can not return a reference to a temporary.
  static const T_COL GetCol(const ENTRY& entry) { return entry.col(); }
  static void SetCol(ENTRY& entry, CF<T_COL> col) { entry.set_col(col); }
};

template <typename ENTRY>
using COL_ACCESSOR = COL_ACCESSOR_IMPL<ENTRY, HasColFunction<ENTRY>(0)>;

template <typename ENTRY>
typename COL_ACCESSOR<ENTRY>::T_COL GetCol(const ENTRY& entry) {
  return COL_ACCESSOR<ENTRY>::GetCol(entry);
}

template <typename ENTRY>
using ENTRY_COL_TYPE = current::decay<typename COL_ACCESSOR<ENTRY>::T_COL>;

template <typename ENTRY>
void SetCol(ENTRY& entry, CF<ENTRY_COL_TYPE<ENTRY>> col) {
  COL_ACCESSOR<ENTRY>::SetCol(entry, col);
}

template <typename T_ROW,
          typename T_COL,
          bool ROW_FITS_UNORDERED_MAP,
          bool ROW_HAS_OPERATOR_LESS,
          bool COL_FITS_UNORDERED_MAP,
          bool COL_HAS_OPERATOR_LESS>
struct RowColImpl {
  static_assert(
      (ROW_FITS_UNORDERED_MAP && COL_FITS_UNORDERED_MAP) || (ROW_HAS_OPERATOR_LESS && COL_HAS_OPERATOR_LESS),
      "Row and column types must either both (be hashable + have `operator==`) or both have `operator<`.");
};

template <typename T_ROW, typename T_COL, bool ROW_HAS_OPERATOR_LESS, bool COL_HAS_OPERATOR_LESS>
struct RowColImpl<T_ROW, T_COL, true, ROW_HAS_OPERATOR_LESS, true, COL_HAS_OPERATOR_LESS> {
  T_ROW row;
  T_COL col;
  RowColImpl(CF<T_ROW> row, CF<T_COL> col) : row(row), col(col) {}
  size_t Hash() const { return HASH_FUNCTION<T_ROW>()(row) ^ HASH_FUNCTION<T_COL>()(col); }
  bool operator==(const RowColImpl& rhs) const { return row == rhs.row && col == rhs.col; }
};

template <typename T_ROW, typename T_COL>
struct RowColWithOperatorLess {
  T_ROW row;
  T_COL col;
  RowColWithOperatorLess(CF<T_ROW> row, CF<T_COL> col) : row(row), col(col) {}
  bool operator<(const RowColWithOperatorLess& rhs) const {
    return std::tie(row, col) < std::tie(rhs.row, rhs.col);
  }
};

template <typename T_ROW, typename T_COL>
struct RowColImpl<T_ROW, T_COL, false, true, false, true> : RowColWithOperatorLess<T_ROW, T_COL> {
  using RowColWithOperatorLess<T_ROW, T_COL>::RowColWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
struct RowColImpl<T_ROW, T_COL, true, true, false, true> : RowColWithOperatorLess<T_ROW, T_COL> {
  using RowColWithOperatorLess<T_ROW, T_COL>::RowColWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
struct RowColImpl<T_ROW, T_COL, false, true, true, true> : RowColWithOperatorLess<T_ROW, T_COL> {
  using RowColWithOperatorLess<T_ROW, T_COL>::RowColWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
using RowCol = RowColImpl<T_ROW,
                          T_COL,
                          FitsAsKeyForUnorderedMap<T_ROW>(),
                          HasOperatorLess<T_ROW>(0),
                          FitsAsKeyForUnorderedMap<T_COL>(),
                          HasOperatorLess<T_COL>(0)>;

}  // namespace sfinae

template <typename TR, typename TC, int Q>
struct MatrixGlobalDeleterPersister : Padawan {
  TR row_to_erase;
  TC col_to_erase;
  virtual ~MatrixGlobalDeleterPersister() {}
  MatrixGlobalDeleterPersister(TR row_to_erase = TR(), TC col_to_erase = TC())
      : row_to_erase(row_to_erase), col_to_erase(col_to_erase) {}
  template <typename A>
  void serialize(A& ar) {
    ar(CEREAL_NVP(row_to_erase), CEREAL_NVP(col_to_erase));
  }
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_CONTAINER_MATRIX_METAPROGRAMMING_H
