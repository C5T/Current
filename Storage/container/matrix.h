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

#ifndef CURRENT_STORAGE_CONTAINER_MATRIX_H
#define CURRENT_STORAGE_CONTAINER_MATRIX_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../TypeSystem/optional.h"
#include "../../Bricks/util/comparators.h"  // For `CurrentHashFunction`.

namespace current {
namespace storage {
namespace container {

template <typename T,
          typename UPDATE_EVENT,
          typename DELETE_EVENT,
          template <typename...> class ROW_MAP,
          template <typename...> class COL_MAP>
class GenericMatrix {
 public:
  using row_t = sfinae::entry_row_t<T>;
  using col_t = sfinae::entry_col_t<T>;
  using whole_matrix_map_t = std::unordered_map<std::pair<row_t, col_t>,
                                                std::unique_ptr<T>,
                                                CurrentHashFunction<std::pair<row_t, col_t>>>;
  using forward_map_t = ROW_MAP<row_t, COL_MAP<col_t, const T*>>;
  using transposed_map_t = COL_MAP<col_t, ROW_MAP<row_t, const T*>>;
  using rest_behavior_t = rest::behavior::Matrix;

  using DEPRECATED_T_(ROW) = row_t;
  using DEPRECATED_T_(COL) = col_t;

  explicit GenericMatrix(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  void Add(const T& object) {
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto row_col = std::make_pair(row, col);
    const auto it = map_.find(row_col);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(UPDATE_EVENT(object),
                           [this, row_col, previous_object]() { DoAdd(row_col, previous_object); });
    } else {
      journal_.LogMutation(UPDATE_EVENT(object), [this, row_col]() { DoErase(row_col); });
    }
    DoAdd(row_col, object);
  }

  void Erase(sfinae::CF<row_t> row, sfinae::CF<col_t> col) {
    const auto row_col = std::make_pair(row, col);
    const auto it = map_.find(row_col);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(DELETE_EVENT(previous_object),
                           [this, row, col, row_col, previous_object]() {
                             auto& placeholder = map_[row_col];
                             placeholder = std::make_unique<T>(previous_object);
                             forward_[row][col] = placeholder.get();
                             transposed_[col][row] = placeholder.get();
                           });
      DoErase(row_col);
    }
  }
  void Erase(const std::pair<row_t, col_t>& key) { Erase(key.first, key.second); }

  ImmutableOptional<T> Get(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    const auto it = map_.find(std::make_pair(row, col));
    if (it != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), it->second.get());
    } else {
      return nullptr;
    }
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoAdd(std::make_pair(row, col), e.data);
  }
  void operator()(const DELETE_EVENT& e) { DoErase(std::make_pair(e.key.first, e.key.second)); }

  template <typename OUTER_KEY, typename INNER_MAP>
  struct InnerAccessor final {
    using INNER_KEY = typename INNER_MAP::key_type;
    const OUTER_KEY key_;
    const INNER_MAP& map_;

    struct Iterator final {
      using iterator_t = typename INNER_MAP::const_iterator;
      iterator_t iterator;
      explicit Iterator(iterator_t iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<INNER_KEY> key() const { return iterator->first; }
      const T& operator*() const { return *iterator->second; }
      const T* operator->() const { return iterator->second; }
    };

    InnerAccessor(OUTER_KEY key, const INNER_MAP& map) : key_(key), map_(map) {}

    bool Empty() const { return map_.empty(); }
    size_t Size() const { return map_.size(); }

    sfinae::CF<OUTER_KEY> key() const { return key_; }

    bool Has(const INNER_KEY& x) const { return map_.find(x) != map_.end(); }

    Iterator begin() const { return Iterator(map_.cbegin()); }
    Iterator end() const { return Iterator(map_.cend()); }
  };

  template <typename OUTER_MAP>
  struct OuterAccessor final {
    using OUTER_KEY = typename OUTER_MAP::key_type;
    using INNER_MAP = typename OUTER_MAP::mapped_type;
    const OUTER_MAP& map_;

    struct OuterIterator final {
      using iterator_t = typename OUTER_MAP::const_iterator;
      iterator_t iterator;
      explicit OuterIterator(iterator_t iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const OuterIterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const OuterIterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<OUTER_KEY> key() const { return iterator->first; }
      InnerAccessor<OUTER_KEY, INNER_MAP> operator*() const {
        return InnerAccessor<OUTER_KEY, INNER_MAP>(iterator->first, iterator->second);
      }
    };

    explicit OuterAccessor(const OUTER_MAP& map) : map_(map) {}

    bool Empty() const { return map_.empty(); }

    size_t Size() const { return map_.size(); }

    bool Has(const OUTER_KEY& x) const { return map_.find(x) != map_.end(); }

    ImmutableOptional<InnerAccessor<OUTER_KEY, INNER_MAP>> operator[](OUTER_KEY key) const {
      const auto iterator = map_.find(key);
      if (iterator != map_.end()) {
        return std::move(std::make_unique<InnerAccessor<OUTER_KEY, INNER_MAP>>(key, iterator->second));
      } else {
        return nullptr;
      }
    }

    OuterIterator begin() const { return OuterIterator(map_.cbegin()); }
    OuterIterator end() const { return OuterIterator(map_.cend()); }
  };

  OuterAccessor<forward_map_t> Rows() const { return OuterAccessor<forward_map_t>(forward_); }

  OuterAccessor<transposed_map_t> Cols() const { return OuterAccessor<transposed_map_t>(transposed_); }

  ImmutableOptional<T> operator[](const std::pair<row_t, col_t>& full_key_as_pair) const {
    const auto cit = map_.find(full_key_as_pair);
    if (cit != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), cit->second.get());
    } else {
      return nullptr;
    }
  }

  // For REST, iterate over all the elemnts of the matrix, in no particular order.
  struct WholeMatrixIterator final {
    using iterator_t = typename whole_matrix_map_t::const_iterator;
    iterator_t iterator;
    explicit WholeMatrixIterator(iterator_t iterator) : iterator(iterator) {}
    void operator++() { ++iterator; }
    bool operator==(const WholeMatrixIterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const WholeMatrixIterator& rhs) const { return !operator==(rhs); }
    const std::pair<row_t, col_t> key() const { return iterator->first; }
    const T& operator*() const { return *iterator->second; }
    const T* operator->() const { return iterator->second; }
  };
  WholeMatrixIterator WholeMatrixBegin() const { return WholeMatrixIterator(map_.begin()); }
  WholeMatrixIterator WholeMatrixEnd() const { return WholeMatrixIterator(map_.end()); }

 private:
  void DoAdd(const std::pair<row_t, col_t>& row_col, const T& object) {
    auto& placeholder = map_[row_col];
    placeholder = std::make_unique<T>(object);
    forward_[row_col.first][row_col.second] = placeholder.get();
    transposed_[row_col.second][row_col.first] = placeholder.get();
  }

  void DoErase(const std::pair<row_t, col_t>& row_col) {
    auto& map_row = forward_[row_col.first];
    map_row.erase(row_col.second);
    if (map_row.empty()) {
      forward_.erase(row_col.first);
    }
    auto& map_col = transposed_[row_col.second];
    map_col.erase(row_col.first);
    if (map_col.empty()) {
      transposed_.erase(row_col.second);
    }
    map_.erase(row_col);
  }

  whole_matrix_map_t map_;
  forward_map_t forward_;
  transposed_map_t transposed_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedMatrix = GenericMatrix<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedMatrix = GenericMatrix<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Ordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedMatrix<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedMatrix"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedMatrix<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedMatrix"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedMatrix;
using current::storage::container::OrderedMatrix;

#endif  // CURRENT_STORAGE_CONTAINER_MATRIX_H
