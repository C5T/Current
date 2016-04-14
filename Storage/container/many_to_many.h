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

#ifndef CURRENT_STORAGE_CONTAINER_MANY_TO_MANY_H
#define CURRENT_STORAGE_CONTAINER_MANY_TO_MANY_H

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
class GenericManyToMany {
 public:
  using row_t = sfinae::entry_row_t<T>;
  using col_t = sfinae::entry_col_t<T>;
  using key_t = std::pair<row_t, col_t>;
  using whole_matrix_map_t = std::unordered_map<std::pair<row_t, col_t>,
                                                std::unique_ptr<T>,
                                                CurrentHashFunction<std::pair<row_t, col_t>>>;
  using forward_map_t = ROW_MAP<row_t, COL_MAP<col_t, const T*>>;
  using transposed_map_t = COL_MAP<col_t, ROW_MAP<row_t, const T*>>;
  using rest_behavior_t = rest::behavior::Matrix;

  using DEPRECATED_T_(ROW) = row_t;
  using DEPRECATED_T_(COL) = col_t;

  explicit GenericManyToMany(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  void Add(const T& object) {
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto key = std::make_pair(row, col);
    const auto cit = map_.find(key);
    if (cit != map_.end()) {
      const T& previous_object = *(cit->second);
      journal_.LogMutation(UPDATE_EVENT(object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
    } else {
      journal_.LogMutation(UPDATE_EVENT(object), [this, key]() { DoErase(key); });
    }
    DoAdd(key, object);
  }

  void Erase(const key_t& key) {
    const auto cit = map_.find(key);
    if (cit != map_.end()) {
      const T& previous_object = *(cit->second);
      journal_.LogMutation(DELETE_EVENT(previous_object),
                           [this, key, previous_object]() { DoAdd(key, previous_object); });
      DoErase(key);
    }
  }
  void Erase(sfinae::CF<row_t> row, sfinae::CF<col_t> col) { Erase(std::make_pair(row, col)); }

  ImmutableOptional<T> operator[](const key_t& key) const {
    const auto cit = map_.find(key);
    if (cit != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), cit->second.get());
    } else {
      return nullptr;
    }
  }
  ImmutableOptional<T> Get(sfinae::CF<T_ROW> row, sfinae::CF<T_COL> col) const {
    return operator[](std::make_pair(row, col));
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoAdd(std::make_pair(row, col), e.data);
  }
  void operator()(const DELETE_EVENT& e) { DoErase(std::make_pair(e.key.first, e.key.second)); }

  template <typename MAP>
  struct IteratorImpl final {
    using iterator_t = typename MAP::const_iterator;
    using key_t = typename MAP::key_type;
    iterator_t iterator;
    explicit IteratorImpl(iterator_t iterator) : iterator(iterator) {}
    void operator++() { ++iterator; }
    bool operator==(const IteratorImpl& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const IteratorImpl& rhs) const { return !operator==(rhs); }
    sfinae::CF<key_t> key() const { return iterator->first; }
    const T& operator*() const { return *iterator->second; }
    const T* operator->() const { return iterator->second; }
  };

  template <typename OUTER_KEY, typename INNER_MAP>
  struct InnerAccessor final {
    using iterator_t = IteratorImpl<INNER_MAP>;
    using INNER_KEY = typename INNER_MAP::key_type;
    const OUTER_KEY key_;
    const INNER_MAP& map_;

    InnerAccessor(OUTER_KEY key, const INNER_MAP& map) : key_(key), map_(map) {}

    bool Empty() const { return map_.empty(); }
    size_t Size() const { return map_.size(); }

    sfinae::CF<OUTER_KEY> key() const { return key_; }

    bool Has(const INNER_KEY& x) const { return map_.find(x) != map_.end(); }

    iterator_t begin() const { return iterator_t(map_.cbegin()); }
    iterator_t end() const { return iterator_t(map_.cend()); }
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

  // For REST, iterate over all the elemnts of the ManyToMany, in no particular order.
  using iterator_t = IteratorImpl<whole_matrix_map_t>;
  iterator_t begin() const { return iterator_t(map_.begin()); }
  iterator_t end() const { return iterator_t(map_.end()); }

 private:
  void DoAdd(const key_t& key, const T& object) {
    auto& placeholder = map_[key];
    placeholder = std::make_unique<T>(object);
    forward_[key.first][key.second] = placeholder.get();
    transposed_[key.second][key.first] = placeholder.get();
  }

  void DoErase(const key_t& key) {
    auto& map_row = forward_[key.first];
    map_row.erase(key.second);
    if (map_row.empty()) {
      forward_.erase(key.first);
    }
    auto& map_col = transposed_[key.second];
    map_col.erase(key.first);
    if (map_col.empty()) {
      transposed_.erase(key.second);
    }
    map_.erase(key);
  }

  whole_matrix_map_t map_;
  forward_map_t forward_;
  transposed_map_t transposed_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedManyToMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedManyToMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Ordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedManyToMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedManyToMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedManyToMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedManyToMany"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedManyToMany;
using current::storage::container::OrderedManyToMany;

#endif  // CURRENT_STORAGE_CONTAINER_MANY_TO_MANY_H
