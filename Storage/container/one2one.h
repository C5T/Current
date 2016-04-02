/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef CURRENT_STORAGE_CONTAINER_ONE2ONE_H
#define CURRENT_STORAGE_CONTAINER_ONE2ONE_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../TypeSystem/optional.h"
#include "../../Bricks/util/comparators.h"  // For `CurrentHashFunction`.

namespace current {
namespace storage {
namespace container {

template <typename T,
          typename T_UPDATE_EVENT,
          typename T_DELETE_EVENT,
          template <typename...> class ROW_MAP,
          template <typename...> class COL_MAP>
class GenericOne2One {
 public:
  using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
  using T_COL = sfinae::ENTRY_COL_TYPE<T>;
  using T_KEY = std::pair<T_ROW, T_COL>;
  using T_ELEMENTS_MAP = std::unordered_map<T_KEY, std::unique_ptr<T>, CurrentHashFunction<T_KEY>>;
  using T_FORWARD_MAP = ROW_MAP<T_ROW, const T*>;
  using T_TRANSPOSED_MAP = COL_MAP<T_COL, const T*>;
  using T_REST_BEHAVIOR = rest::behavior::Matrix;

  explicit GenericOne2One(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  // Adds specified object, removes and/or overwrites all existing entries
  // with row and/or col value matching the corresponding values of the new entry
  void Add(const T& object) {
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto key = std::make_pair(row, col);
    const auto it = map_.find(key);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(T_UPDATE_EVENT(object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
    } else {
      const auto it_row = forward_.find(row);
      const auto it_col = transposed_.find(col);
      const bool row_occupied = (it_row != forward_.end());
      const bool col_occupied = (it_col != transposed_.end());
      if (row_occupied && col_occupied) {
        const T previous_object_same_row = *(it_row->second);
        const T previous_object_same_col = *(it_col->second);
        const auto key_same_row = std::make_pair(row, sfinae::GetCol(previous_object_same_row));
        const auto key_same_col = std::make_pair(sfinae::GetRow(previous_object_same_col), col);
        journal_.LogMutation(T_DELETE_EVENT(previous_object_same_row),
                             [this, key_same_row, previous_object_same_row]() {
                               DoAdd(key_same_row, previous_object_same_row);
                             });
        journal_.LogMutation(T_DELETE_EVENT(previous_object_same_col),
                             [this, key_same_col, previous_object_same_col]() {
                               DoAdd(key_same_col, previous_object_same_col);
                             });
        DoErase(key_same_row);
        DoErase(key_same_col);
      } else if (row_occupied || col_occupied) {
        const T previous_object = row_occupied ? *(it_row->second) : *(it_col->second);
        const auto previous_key =
            std::make_pair(sfinae::GetRow(previous_object), sfinae::GetCol(previous_object));
        journal_.LogMutation(T_DELETE_EVENT(previous_object),
                             [this, previous_key, previous_object]() { DoAdd(previous_key, previous_object); });
        DoErase(previous_key);
      }
    }
    journal_.LogMutation(T_UPDATE_EVENT(object), [this, key]() { DoErase(key); });
    DoAdd(key, object);
  }

  void Erase(const T_KEY& key) {
    const auto it = map_.find(key);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(T_DELETE_EVENT(previous_object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
      DoErase(key);
    }
  }
  void Erase(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) { Erase(std::make_pair(row, col)); }

  void EraseRow(sfinae::CF<T_ROW> row) {
    const auto it = forward_.find(row);
    if (it != forward_.end()) {
      const T previous_object = *(it->second);
      const auto key = std::make_pair(row, sfinae::GetCol(previous_object));
      journal_.LogMutation(T_DELETE_EVENT(previous_object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
      DoErase(key);
    }
  }

  void EraseCol(sfinae::CF<T_COL> col) {
    const auto it = transposed_.find(col);
    if (it != transposed_.end()) {
      const T previous_object = *(it->second);
      const auto key = std::make_pair(sfinae::GetRow(previous_object), col);
      journal_.LogMutation(T_DELETE_EVENT(previous_object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
      DoErase(key);
    }
  }

  ImmutableOptional<T> operator[](const T_KEY& key) const {
    const auto it = map_.find(key);
    if (it != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), it->second.get());
    } else {
      return nullptr;
    }
  }
  ImmutableOptional<T> Get(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const {
    return operator[](std::make_pair(row, col));
  }
  ImmutableOptional<T> GetRow(sfinae::CF<T_ROW> row) const {
    const auto it = forward_.find(row);
    if (it != forward_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), it->second);
    } else {
      return nullptr;
    }
  }
  ImmutableOptional<T> GetCol(sfinae::CF<T_COL> col) const {
    const auto it = transposed_.find(col);
    if (it != transposed_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), it->second);
    } else {
      return nullptr;
    }
  }

  bool CanAdd(const T_KEY& key) const {
    return forward_.find(key.first) == forward_.end() && transposed_.find(key.second) == transposed_.end();
  }
  bool CanAdd(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const { return CanAdd(std::make_pair(row, col)); }

  void operator()(const T_UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoAdd(std::make_pair(row, col), e.data);
  }
  void operator()(const T_DELETE_EVENT& e) { DoErase(std::make_pair(e.key.first, e.key.second)); }

  template <typename T_MAP>
  struct MapAccessor final {
    using T_KEY = typename T_MAP::key_type;
    const T_MAP& map_;

    struct Iterator final {
      using T_ITERATOR = typename T_MAP::const_iterator;
      T_ITERATOR iterator_;
      explicit Iterator(T_ITERATOR iterator) : iterator_(iterator) {}
      void operator++() { ++iterator_; }
      bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<T_KEY> key() const { return iterator_->first; }
      const T& operator*() const { return *iterator_->second; }
      const T* operator->() const { return iterator_->second; }
    };

    explicit MapAccessor(const T_MAP& map) : map_(map) {}

    bool Empty() const { return map_.empty(); }

    size_t Size() const { return map_.size(); }

    bool Has(const T_KEY& x) const { return map_.find(x) != map_.end(); }

    Iterator begin() const { return Iterator(map_.cbegin()); }
    Iterator end() const { return Iterator(map_.cend()); }
  };

  const MapAccessor<T_FORWARD_MAP> Rows() const { return MapAccessor<T_FORWARD_MAP>(forward_); }

  const MapAccessor<T_TRANSPOSED_MAP> Cols() const { return MapAccessor<T_TRANSPOSED_MAP>(transposed_); }

  struct Iterator final {
    using T_ITERATOR = typename T_ELEMENTS_MAP::const_iterator;
    T_ITERATOR iterator_;
    explicit Iterator(T_ITERATOR iterator) : iterator_(iterator) {}
    void operator++() { ++iterator_; }
    bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    const T_KEY key() const { return iterator_->first; }
    const T& operator*() const { return *iterator_->second; }
    const T* operator->() const { return iterator_->second; }
  };
  Iterator Begin() const { return Iterator(map_.begin()); }
  Iterator End() const { return Iterator(map_.end()); }

 private:
  void DoErase(const T_KEY& key) {
    forward_.erase(key.first);
    transposed_.erase(key.second);
    map_.erase(key);
  }

  void DoAdd(const T_KEY& key, const T& object) {
    auto& placeholder = map_[key];
    placeholder = std::make_unique<T>(object);
    forward_[key.first] = placeholder.get();
    transposed_[key.second] = placeholder.get();
  }

  T_ELEMENTS_MAP map_;
  T_FORWARD_MAP forward_;
  T_TRANSPOSED_MAP transposed_;
  MutationJournal& journal_;
};

template <typename T, typename T_UPDATE_EVENT, typename T_DELETE_EVENT>
using UnorderedOne2One = GenericOne2One<T, T_UPDATE_EVENT, T_DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename T_UPDATE_EVENT, typename T_DELETE_EVENT>
using OrderedOne2One = GenericOne2One<T, T_UPDATE_EVENT, T_DELETE_EVENT, Ordered, Ordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOne2One<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOne2One"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOne2One<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOne2One"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedOne2One;
using current::storage::container::OrderedOne2One;

#endif  // CURRENT_STORAGE_CONTAINER_ONE2ONE_H
