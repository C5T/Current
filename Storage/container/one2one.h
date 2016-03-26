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
          template <typename ...> class ROW_MAP,
          template <typename ...> class COL_MAP>
class GenericOne2One {
 public:
  using T_ROW = sfinae::ENTRY_ROW_TYPE<T>;
  using T_COL = sfinae::ENTRY_COL_TYPE<T>;
  using T_RCPAIR = std::pair<T_ROW, T_COL>;
  using T_WHOLE_MAP = std::unordered_map<T_RCPAIR,
                                                std::unique_ptr<T>,
                                                CurrentHashFunction<T_RCPAIR>>;
  using T_FORWARD_MAP = ROW_MAP<T_ROW, const T*>;
  using T_TRANSPOSED_MAP = COL_MAP<T_COL, const T*>;
  using T_REST_BEHAVIOR = rest::behavior::Matrix;

  explicit GenericOne2One(MutationJournal& journal) : journal_(journal) {}
  
  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  void Add(const T& object) {
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto row_col = std::make_pair(row, col);
    const auto it = map_.find(row_col);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(T_UPDATE_EVENT(object),
                           [this, row_col, previous_object]() { DoAdd(row_col, previous_object); });
    } else {
      const auto it_row = forward_.find(row);
      const auto it_col = transposed_.find(col);
      bool row_occupied = it_row != forward_.end();
      bool col_occupied = it_col != transposed_.end();
      // TODO: need to simplify, this is too complex
      if (!row_occupied && !col_occupied) {
        journal_.LogMutation(T_UPDATE_EVENT(object),
                             [this, row_col]() { DoErase(row_col); });
      } else if (row_occupied && col_occupied) {
        const T prev_object1 = *(it_row->second);
        const T prev_object2 = *(it_col->second);
        const auto row_col1 = std::make_pair(row, sfinae::GetCol(prev_object1));
        const auto row_col2 = std::make_pair(sfinae::GetRow(prev_object2), col);
        journal_.LogMutation(T_UPDATE_EVENT(object),
                             [this, row_col, row_col1, prev_object1, row_col2, prev_object2] {
                               DoErase(row_col);
                               DoAdd(row_col1, prev_object1);
                               DoAdd(row_col2, prev_object2);
                             });
        DoErase(row_col1);
        DoErase(row_col2);
      } else {
        const T prev_object = row_occupied ? *(it_row->second) : *(it_col->second);
        const auto prev_row_col = std::make_pair(sfinae::GetRow(prev_object),
                                                 sfinae::GetCol(prev_object));
        journal_.LogMutation(T_UPDATE_EVENT(object),
                             [this, row_col, prev_row_col, prev_object]() {
                               DoErase(row_col);
                               DoAdd(prev_row_col, prev_object);
                             });
        DoErase(prev_row_col);
      }
    }
    DoAdd(row_col, object);
  }
  
  void Erase(const T_RCPAIR& row_col) {
    auto it = map_.find(row_col);
    if (it != map_.end()) {
      const T previous_object = *(it->second);
      journal_.LogMutation(T_UPDATE_EVENT(previous_object),
                           [this, row_col, previous_object]() {
                             DoAdd(row_col, previous_object);
                           });
      DoErase(row_col);
    }
  }
  void Erase(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) { Erase(std::make_pair(row, col)); }
  
  ImmutableOptional<T> operator[](const T_RCPAIR& row_col) const {
    const auto it = map_.find(row_col);
    if (it == map_.end())
      return nullptr;
    return ImmutableOptional<T>(FromBarePointer(), it->second.get());
  }
  ImmutableOptional<T> Get(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const {
    return operator[](std::make_pair(row, col));
  }

  void operator()(const T_UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoAdd(std::make_pair(row, col), e.data); // ??? is it enough? What about integrity?
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
    using T_ITERATOR = typename T_WHOLE_MAP::const_iterator;
    T_ITERATOR iterator_;
    explicit Iterator(T_ITERATOR iterator) : iterator_(iterator) {}
    void operator++() { ++iterator_; }
    bool operator==(const Iterator& rhs) const { return iterator_ == rhs.iterator_; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    const T_RCPAIR key() const { return iterator_->first; }
    const T& operator*() const { return *iterator_->second; }
    const T* operator->() const { return iterator_->second; }
  };
  Iterator Begin() const { return Iterator(map_.begin()); }
  Iterator End() const { return Iterator(map_.end()); }

 private:
  void DoErase(const T_RCPAIR& row_col) {
    forward_.erase(row_col.first);
    transposed_.erase(row_col.second);
    map_.erase(row_col);
  }
  
  void DoAdd(const T_RCPAIR& row_col, const T& object) {
    auto& placeholder = map_[row_col];
    placeholder = std::make_unique<T>(object);
    forward_[row_col.first] = placeholder.get();
    transposed_[row_col.second] = placeholder.get();
  }
  
  T_WHOLE_MAP map_;
  T_FORWARD_MAP forward_;
  T_TRANSPOSED_MAP transposed_;
	MutationJournal& journal_;
};

template <typename T, typename T_UPDATE_EVENT, typename T_DELETE_EVENT>
using UnorderedOne2One = GenericOne2One<T, T_UPDATE_EVENT, T_DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename T_UPDATE_EVENT, typename T_DELETE_EVENT>
using OrderedOne2One = GenericOne2One<T, T_UPDATE_EVENT, T_DELETE_EVENT, Ordered, Ordered>;

}  // namespace container

template <typename T, typename E1, typename E2> // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOne2One<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOne2One"; }
};

template <typename T, typename E1, typename E2> // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOne2One<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOne2One"; }
};

}  // namespace storage
}  // namespace current


using current::storage::container::UnorderedOne2One;
using current::storage::container::OrderedOne2One;

#endif  // CURRENT_STORAGE_CONTAINER_ONE2ONE_H
