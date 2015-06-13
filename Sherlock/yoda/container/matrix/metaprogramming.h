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

#include <utility>

#include "../../metaprogramming.h"
#include "../../sfinae.h"
#include "../../../../Bricks/template/pod.h"
#include "../../../../Bricks/template/decay.h"

namespace yoda {

namespace sfinae {

template <typename T>
using CF = bricks::copy_free<T>;

// Matrix row and column type extractors, getters and setters.
// Support two access methods:
// - `.row / .col` data members,
// - `.row() / set_row() / .col() / .set_col()` methods.
template <typename T_ENTRY>
constexpr bool HasRowFunction(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasRowFunction(int) -> decltype(std::declval<const T_ENTRY>().row(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_ROW_FUNCTION>
struct ROW_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct ROW_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().row) T_ROW;
  static CF<T_ROW> GetRow(const T_ENTRY& entry) { return entry.row; }
  static void SetRow(T_ENTRY& entry, CF<T_ROW> row) { entry.row = row; }
};

template <typename T_ENTRY>
struct ROW_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().row()) T_ROW;
  // Can not return a reference to a temporary.
  static const T_ROW GetRow(const T_ENTRY& entry) { return entry.row(); }
  static void SetRow(T_ENTRY& entry, CF<T_ROW> row) { entry.set_row(row); }
};

template <typename T_ENTRY>
using ROW_ACCESSOR = ROW_ACCESSOR_IMPL<T_ENTRY, HasRowFunction<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename ROW_ACCESSOR<T_ENTRY>::T_ROW GetRow(const T_ENTRY& entry) {
  return ROW_ACCESSOR<T_ENTRY>::GetRow(entry);
}

template <typename T_ENTRY>
using ENTRY_ROW_TYPE = bricks::decay<typename ROW_ACCESSOR<T_ENTRY>::T_ROW>;

template <typename T_ENTRY>
void SetRow(T_ENTRY& entry, CF<ENTRY_ROW_TYPE<T_ENTRY>> row) {
  ROW_ACCESSOR<T_ENTRY>::SetRow(entry, row);
}

template <typename T_ENTRY>
constexpr bool HasColFunction(char) {
  return false;
}

template <typename T_ENTRY>
constexpr auto HasColFunction(int) -> decltype(std::declval<const T_ENTRY>().col(), bool()) {
  return true;
}

template <typename T_ENTRY, bool HAS_COL_FUNCTION>
struct COL_ACCESSOR_IMPL {};

template <typename T_ENTRY>
struct COL_ACCESSOR_IMPL<T_ENTRY, false> {
  typedef decltype(std::declval<T_ENTRY>().col) T_COL;
  static CF<T_COL> GetCol(const T_ENTRY& entry) { return entry.col; }
  static void SetCol(T_ENTRY& entry, T_COL col) { entry.col = col; }
};

template <typename T_ENTRY>
struct COL_ACCESSOR_IMPL<T_ENTRY, true> {
  typedef decltype(std::declval<T_ENTRY>().col()) T_COL;
  // Can not return a reference to a temporary.
  static const T_COL GetCol(const T_ENTRY& entry) { return entry.col(); }
  static void SetCol(T_ENTRY& entry, CF<T_COL> col) { entry.set_col(col); }
};

template <typename T_ENTRY>
using COL_ACCESSOR = COL_ACCESSOR_IMPL<T_ENTRY, HasColFunction<T_ENTRY>(0)>;

template <typename T_ENTRY>
typename COL_ACCESSOR<T_ENTRY>::T_COL GetCol(const T_ENTRY& entry) {
  return COL_ACCESSOR<T_ENTRY>::GetCol(entry);
}

template <typename T_ENTRY>
using ENTRY_COL_TYPE = bricks::decay<typename COL_ACCESSOR<T_ENTRY>::T_COL>;

template <typename T_ENTRY>
void SetCol(T_ENTRY& entry, CF<ENTRY_COL_TYPE<T_ENTRY>> col) {
  COL_ACCESSOR<T_ENTRY>::SetCol(entry, col);
}

template <typename T_ROW,
          typename T_COL,
          bool ROW_FITS_UNORDERED_MAP,
          bool ROW_HAS_OPERATOR_LESS,
          bool COL_FITS_UNORDERED_MAP,
          bool COL_HAS_OPERATOR_LESS>
struct MapPairKeyImpl {
  static_assert(
      (ROW_FITS_UNORDERED_MAP && COL_FITS_UNORDERED_MAP) || (ROW_HAS_OPERATOR_LESS && COL_HAS_OPERATOR_LESS),
      "Row and column types must either both (be hashable + have `operator==`) or both have `operator<`.");
};

template <typename T_ROW, typename T_COL, bool ROW_HAS_OPERATOR_LESS, bool COL_HAS_OPERATOR_LESS>
struct MapPairKeyImpl<T_ROW, T_COL, true, ROW_HAS_OPERATOR_LESS, true, COL_HAS_OPERATOR_LESS> {
  T_ROW row;
  T_COL col;
  MapPairKeyImpl(CF<T_ROW> row, CF<T_COL> col) : row(row), col(col) {}
  size_t Hash() const { return T_HASH_FUNCTION<T_ROW>()(row) ^ T_HASH_FUNCTION<T_COL>()(col); }
  bool operator==(const MapPairKeyImpl& rhs) const { return row == rhs.row && col == rhs.col; }
};

template <typename T_ROW, typename T_COL>
struct MapPairKeyWithOperatorLess {
  T_ROW row;
  T_COL col;
  MapPairKeyWithOperatorLess(CF<T_ROW> row, CF<T_COL> col) : row(row), col(col) {}
  bool operator<(const MapPairKeyWithOperatorLess& rhs) const { return row < rhs.row && col < rhs.col; }
};

template <typename T_ROW, typename T_COL>
struct MapPairKeyImpl<T_ROW, T_COL, false, true, false, true> : MapPairKeyWithOperatorLess<T_ROW, T_COL> {
  using MapPairKeyWithOperatorLess<T_ROW, T_COL>::MapPairKeyWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
struct MapPairKeyImpl<T_ROW, T_COL, true, true, false, true> : MapPairKeyWithOperatorLess<T_ROW, T_COL> {
  using MapPairKeyWithOperatorLess<T_ROW, T_COL>::MapPairKeyWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
struct MapPairKeyImpl<T_ROW, T_COL, false, true, true, true> : MapPairKeyWithOperatorLess<T_ROW, T_COL> {
  using MapPairKeyWithOperatorLess<T_ROW, T_COL>::MapPairKeyWithOperatorLess;
};

template <typename T_ROW, typename T_COL>
using MapPairKey = MapPairKeyImpl<T_ROW,
                                  T_COL,
                                  FitsAsKeyForUnorderedMap<T_ROW>(),
                                  HasOperatorLess<T_ROW>(0),
                                  FitsAsKeyForUnorderedMap<T_COL>(),
                                  HasOperatorLess<T_COL>(0)>;

}  // namespace sfinae
}  // namespace yoda

#endif  // SHERLOCK_YODA_CONTAINER_MATRIX_METAPROGRAMMING_H
