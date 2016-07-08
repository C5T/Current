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
#include "../../Bricks/util/iterator.h"     // For `GenericMapIterator` and `GenericMapAccessor`.
#include "../../Bricks/util/singleton.h"

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
  using whole_matrix_map_t = std::unordered_map<key_t, std::unique_ptr<T>, CurrentHashFunction<key_t>>;
  using row_elements_map_t = COL_MAP<col_t, const T*>;
  using col_elements_map_t = ROW_MAP<row_t, const T*>;
  using forward_map_t = ROW_MAP<row_t, row_elements_map_t>;
  using transposed_map_t = COL_MAP<col_t, col_elements_map_t>;
  using rest_behavior_t = rest::behavior::Matrix;

  GenericManyToMany(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  void Add(const T& object) {
    const auto now = current::time::Now();
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto key = std::make_pair(row, col);
    const auto map_cit = map_.find(key);
    const auto lm_cit = last_modified_.find(key);
    if (map_cit != map_.end()) {
      const T& previous_object = *(map_cit->second);
      assert(lm_cit != last_modified_.end());
      const auto previous_timestamp = lm_cit->second;
      journal_.LogMutation(UPDATE_EVENT(now, object),
                           [this, key, previous_object, previous_timestamp]() {
                             DoUpdateWithLastModified(previous_timestamp, key, previous_object);
                           });
    } else {
      if (lm_cit != last_modified_.end()) {
        const auto previous_timestamp = lm_cit->second;
        journal_.LogMutation(
            UPDATE_EVENT(now, object),
            [this, key, previous_timestamp]() { DoEraseWithLastModified(previous_timestamp, key); });
      } else {
        journal_.LogMutation(UPDATE_EVENT(now, object),
                             [this, key]() {
                               DoEraseWithoutTouchingLastModified(key);
                               last_modified_.erase(key);
                             });
      }
    }
    DoUpdateWithLastModified(now, key, object);
  }

  // Here and below pass the key by a const reference, as `key_t` is an `std::pair<row_t, col_t>`.
  void Erase(const key_t& key) {
    const auto now = current::time::Now();
    const auto map_cit = map_.find(key);
    if (map_cit != map_.end()) {
      const T& previous_object = *(map_cit->second);
      const auto lm_cit = last_modified_.find(key);
      assert(lm_cit != last_modified_.end());
      const auto previous_timestamp = lm_cit->second;
      journal_.LogMutation(DELETE_EVENT(now, previous_object),
                           [this, key, previous_object, previous_timestamp]() {
                             DoUpdateWithLastModified(previous_timestamp, key, previous_object);
                           });
      DoEraseWithLastModified(now, key);
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
  ImmutableOptional<T> Get(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    return operator[](std::make_pair(row, col));
  }

  ImmutableOptional<std::chrono::microseconds> LastModified(const key_t& key) const {
    const auto cit = last_modified_.find(key);
    if (cit != last_modified_.end()) {
      return cit->second;
    } else {
      return nullptr;
    }
  }
  ImmutableOptional<std::chrono::microseconds> LastModified(sfinae::CF<row_t> row,
                                                            sfinae::CF<col_t> col) const {
    return LastModified(std::make_pair(row, col));
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoUpdateWithLastModified(e.us, std::make_pair(row, col), e.data);
  }
  void operator()(const DELETE_EVENT& e) {
    DoEraseWithLastModified(e.us, std::make_pair(e.key.first, e.key.second));
  }

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
      GenericMapAccessor<INNER_MAP> operator*() const {
        return GenericMapAccessor<INNER_MAP>(iterator->second);
      }
    };

    explicit OuterAccessor(const OUTER_MAP& map) : map_(map) {}

    bool Empty() const { return map_.empty(); }

    size_t Size() const { return map_.size(); }

    bool Has(const OUTER_KEY& x) const { return map_.find(x) != map_.end(); }

    ImmutableOptional<GenericMapAccessor<INNER_MAP>> operator[](OUTER_KEY key) const {
      const auto iterator = map_.find(key);
      if (iterator != map_.end()) {
        return std::move(std::make_unique<GenericMapAccessor<INNER_MAP>>(iterator->second));
      } else {
        return nullptr;
      }
    }

    OuterIterator begin() const { return OuterIterator(map_.cbegin()); }
    OuterIterator end() const { return OuterIterator(map_.cend()); }
  };

  OuterAccessor<forward_map_t> Rows() const { return OuterAccessor<forward_map_t>(forward_); }

  OuterAccessor<transposed_map_t> Cols() const { return OuterAccessor<transposed_map_t>(transposed_); }

  GenericMapAccessor<row_elements_map_t> Row(sfinae::CF<row_t> row) const {
    const auto cit = forward_.find(row);
    return GenericMapAccessor<row_elements_map_t>(
        cit != forward_.end() ? cit->second : current::ThreadLocalSingleton<row_elements_map_t>());
  }

  GenericMapAccessor<col_elements_map_t> Col(sfinae::CF<col_t> col) const {
    const auto cit = transposed_.find(col);
    return GenericMapAccessor<col_elements_map_t>(
        cit != transposed_.end() ? cit->second : current::ThreadLocalSingleton<col_elements_map_t>());
  }

  // For REST, iterate over all the elements of the ManyToMany, in no particular order.
  using iterator_t = GenericMapIterator<whole_matrix_map_t>;
  iterator_t begin() const { return iterator_t(map_.begin()); }
  iterator_t end() const { return iterator_t(map_.end()); }

 private:
  void DoUpdateWithLastModified(std::chrono::microseconds us, const key_t& key, const T& object) {
    auto& placeholder = map_[key];
    placeholder = std::make_unique<T>(object);
    forward_[key.first][key.second] = placeholder.get();
    transposed_[key.second][key.first] = placeholder.get();
    last_modified_[key] = us;
  }

  void DoEraseWithoutTouchingLastModified(const key_t& key) {
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

  void DoEraseWithLastModified(std::chrono::microseconds us, const key_t& key) {
    DoEraseWithoutTouchingLastModified(key);
    last_modified_[key] = us;
  }

  whole_matrix_map_t map_;
  forward_map_t forward_;
  transposed_map_t transposed_;
  std::unordered_map<key_t, std::chrono::microseconds, CurrentHashFunction<key_t>> last_modified_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedManyToUnorderedMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedManyToOrderedMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedManyToOrderedMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedManyToUnorderedMany = GenericManyToMany<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Unordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedManyToUnorderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedManyToUnorderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedManyToOrderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedManyToOrderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedManyToOrderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedManyToOrderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedManyToUnorderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedManyToUnorderedMany"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedManyToUnorderedMany;
using current::storage::container::OrderedManyToOrderedMany;
using current::storage::container::UnorderedManyToOrderedMany;
using current::storage::container::OrderedManyToUnorderedMany;

#endif  // CURRENT_STORAGE_CONTAINER_MANY_TO_MANY_H
