/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef CURRENT_STORAGE_CONTAINER_ONE_TO_MANY_H
#define CURRENT_STORAGE_CONTAINER_ONE_TO_MANY_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../typesystem/optional.h"
#include "../../bricks/util/comparators.h"  // For `GenericHashFunction`.
#include "../../bricks/util/iterator.h"     // For `GenericMapIterator` and `GenericMapAccessor`.
#include "../../bricks/util/singleton.h"

namespace current {
namespace storage {
namespace container {

template <typename T,
          typename UPDATE_EVENT,
          typename DELETE_EVENT,
          template <typename...> class ROW_MAP,
          template <typename...> class COL_MAP>
class GenericOneToMany {
 public:
  using entry_t = T;
  using row_t = sfinae::entry_row_t<T>;
  using col_t = sfinae::entry_col_t<T>;
  using key_t = std::pair<row_t, col_t>;
  using elements_map_t = std::unordered_map<key_t, std::unique_ptr<T>, GenericHashFunction<key_t>>;
  using row_elements_map_t = COL_MAP<col_t, const T*>;
  using forward_map_t = ROW_MAP<row_t, row_elements_map_t>;
  using transposed_map_t = row_elements_map_t;
  using semantics_t = storage::semantics::OneToMany;

  GenericOneToMany(const std::string& field_name, MutationJournal& journal)
      : field_name_(field_name), journal_(journal) {}

  const std::string& FieldName() const { return field_name_; }

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  bool Has(sfinae::CF<key_t> key) const {
    return map_.find(key) != map_.end();
  }
  bool Has(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    return map_.find(key_t(row, col)) != map_.end();
  }

  // Adds specified object and overwrites existing one if it has the same row and col.
  // Removes all other existing objects with the same col.
  void Add(const T& object) {
    // `now` can be updated to minimize the number of `Now()` calls and keep the order of the timestamps.
    auto now = current::time::Now();
    const auto row = sfinae::GetRow(object);
    const auto col = sfinae::GetCol(object);
    const auto key = std::make_pair(row, col);
    const auto map_cit = map_.find(key);
    const auto lm_cit = last_modified_.find(key);
    if (map_cit != map_.end()) {
      const T& previous_object = *(map_cit->second);
      CURRENT_ASSERT(lm_cit != last_modified_.end());
      const auto previous_timestamp = lm_cit->second;
      journal_.LogMutation(UPDATE_EVENT(now, object),
                           [this, key, previous_object, previous_timestamp]() {
                             DoUpdateWithLastModified(previous_timestamp, key, previous_object);
                           });
    } else {
      const auto transposed_cit = transposed_.find(col);
      if (transposed_cit != transposed_.end()) {
        const T& conflicting_object = *(transposed_cit->second);
        const auto conflicting_object_key = std::make_pair(sfinae::GetRow(conflicting_object), col);
        const auto conflicting_object_lm_cit = last_modified_.find(conflicting_object_key);
        CURRENT_ASSERT(conflicting_object_lm_cit != last_modified_.end());
        const auto conflicting_object_timestamp = conflicting_object_lm_cit->second;
        journal_.LogMutation(DELETE_EVENT(now, conflicting_object),
                             [this, conflicting_object_key, conflicting_object, conflicting_object_timestamp]() {
                               DoUpdateWithLastModified(
                                   conflicting_object_timestamp, conflicting_object_key, conflicting_object);
                             });
        DoEraseWithLastModified(now, conflicting_object_key);
        now = current::time::Now();
      }
      if (lm_cit != last_modified_.end()) {
        const auto previous_timestamp = lm_cit->second;
        journal_.LogMutation(UPDATE_EVENT(now, object),
                             [this, key, previous_timestamp]() { DoEraseWithLastModified(previous_timestamp, key); });
      } else {
        journal_.LogMutation(UPDATE_EVENT(now, object),
                             [this, key]() {
                               last_modified_.erase(key);
                               DoEraseWithoutTouchingLastModified(key);
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
      CURRENT_ASSERT(lm_cit != last_modified_.end());
      const auto previous_timestamp = lm_cit->second;
      journal_.LogMutation(DELETE_EVENT(now, previous_object),
                           [this, key, previous_object, previous_timestamp]() {
                             DoUpdateWithLastModified(previous_timestamp, key, previous_object);
                           });
      DoEraseWithLastModified(now, key);
    }
  }
  void Erase(sfinae::CF<row_t> row, sfinae::CF<col_t> col) { Erase(std::make_pair(row, col)); }

  void EraseCol(sfinae::CF<col_t> col) {
    const auto now = current::time::Now();
    const auto map_cit = transposed_.find(col);
    if (map_cit != transposed_.end()) {
      const T& previous_object = *(map_cit->second);
      const auto key = std::make_pair(sfinae::GetRow(previous_object), col);
      const auto lm_cit = last_modified_.find(key);
      CURRENT_ASSERT(lm_cit != last_modified_.end());
      const auto previous_timestamp = lm_cit->second;
      journal_.LogMutation(DELETE_EVENT(now, previous_object),
                           [this, key, previous_object, previous_timestamp]() {
                             DoUpdateWithLastModified(previous_timestamp, key, previous_object);
                           });
      DoEraseWithLastModified(now, key);
    }
  }

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
  ImmutableOptional<T> GetEntryFromCol(sfinae::CF<col_t> col) const {
    const auto cit = transposed_.find(col);
    if (cit != transposed_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), cit->second);
    } else {
      return nullptr;
    }
  }

  ImmutableOptional<std::chrono::microseconds> LastModified(const key_t& key) const {
    const auto cit = last_modified_.find(key);
    if (cit != last_modified_.end()) {
      return cit->second;
    } else {
      return nullptr;
    }
  }
  ImmutableOptional<std::chrono::microseconds> LastModified(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    return LastModified(std::make_pair(row, col));
  }

  bool DoesNotConflict(const key_t& key) const { return transposed_.find(key.second) == transposed_.end(); }
  bool DoesNotConflict(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    return DoesNotConflict(std::make_pair(row, col));
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoUpdateWithLastModified(e.us, std::make_pair(row, col), e.data);
  }
  void operator()(const DELETE_EVENT& e) { DoEraseWithLastModified(e.us, std::make_pair(e.key.first, e.key.second)); }

  template <typename ROWS_MAP>
  struct RowsAccessor final {
    using key_t = typename ROWS_MAP::key_type;
    using elements_map_t = typename ROWS_MAP::mapped_type;
    const ROWS_MAP& map_;

    struct RowsIterator final {
      using iterator_t = typename ROWS_MAP::const_iterator;
      iterator_t iterator;
      explicit RowsIterator(iterator_t iterator) : iterator(iterator) {}
      void operator++() { ++iterator; }
      bool operator==(const RowsIterator& rhs) const { return iterator == rhs.iterator; }
      bool operator!=(const RowsIterator& rhs) const { return !operator==(rhs); }
      sfinae::CF<key_t> OuterKeyForPartialHypermediaCollectionView() const { return iterator->first; }
      size_t TotalElementsForHypermediaCollectionView() const { return iterator->second.size(); }
      using value_t = GenericMapAccessor<elements_map_t>;
      void has_range_element_t() {}
      using range_element_t = GenericMapAccessor<elements_map_t>;
      GenericMapAccessor<elements_map_t> operator*() const {
        return GenericMapAccessor<elements_map_t>(iterator->second);
      }
    };

    explicit RowsAccessor(const ROWS_MAP& map) : map_(map) {}

    bool Empty() const { return map_.empty(); }
    size_t Size() const { return map_.size(); }

    bool Has(const key_t& x) const { return map_.find(x) != map_.end(); }

    ImmutableOptional<GenericMapAccessor<elements_map_t>> operator[](key_t key) const {
      const auto iterator = map_.find(key);
      if (iterator != map_.end()) {
        return std::move(std::make_unique<GenericMapAccessor<elements_map_t>>(iterator->second));
      } else {
        return nullptr;
      }
    }

    RowsIterator begin() const { return RowsIterator(map_.cbegin()); }
    RowsIterator end() const { return RowsIterator(map_.cend()); }
  };

  using rows_outer_accessor_t = RowsAccessor<forward_map_t>;
  rows_outer_accessor_t Rows() const { return RowsAccessor<forward_map_t>(forward_); }

  using cols_outer_accessor_t = GenericMapAccessor<transposed_map_t>;
  cols_outer_accessor_t Cols() const { return GenericMapAccessor<transposed_map_t>(transposed_); }

  GenericMapAccessor<row_elements_map_t> Row(sfinae::CF<row_t> row) const {
    const auto cit = forward_.find(row);
    return GenericMapAccessor<row_elements_map_t>(
        cit != forward_.end() ? cit->second : current::ThreadLocalSingleton<row_elements_map_t>());
  }

  // For REST, iterate over all the elements of the OneToMany, in no particular order.
  // TODO(dkorolev): Revisit whether this semantics is the right one.
  using iterator_t = GenericMapIterator<elements_map_t>;
  iterator_t begin() const { return iterator_t(map_.begin()); }
  iterator_t end() const { return iterator_t(map_.end()); }

 private:
  void DoUpdateWithLastModified(std::chrono::microseconds us, const key_t& key, const T& object) {
    last_modified_[key] = us;
    auto& placeholder = map_[key];
    placeholder = std::make_unique<T>(object);
    forward_[key.first][key.second] = placeholder.get();
    transposed_[key.second] = placeholder.get();
  }

  void DoEraseWithoutTouchingLastModified(const key_t& key) {
    auto& map_row = forward_[key.first];
    map_row.erase(key.second);
    if (map_row.empty()) {
      forward_.erase(key.first);
    }
    transposed_.erase(key.second);
    map_.erase(key);
  }

  void DoEraseWithLastModified(std::chrono::microseconds us, const key_t& key) {
    last_modified_[key] = us;
    DoEraseWithoutTouchingLastModified(key);
  }

  const std::string field_name_;
  elements_map_t map_;
  forward_map_t forward_;
  transposed_map_t transposed_;
  std::unordered_map<key_t, std::chrono::microseconds, GenericHashFunction<key_t>> last_modified_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedOneToUnorderedMany = GenericOneToMany<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedOneToOrderedMany = GenericOneToMany<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedOneToOrderedMany = GenericOneToMany<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedOneToUnorderedMany = GenericOneToMany<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Unordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOneToUnorderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOneToUnorderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOneToOrderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOneToOrderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOneToOrderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOneToOrderedMany"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOneToUnorderedMany<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOneToUnorderedMany"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedOneToUnorderedMany;
using current::storage::container::OrderedOneToOrderedMany;
using current::storage::container::UnorderedOneToOrderedMany;
using current::storage::container::OrderedOneToUnorderedMany;

#endif  // CURRENT_STORAGE_CONTAINER_ONE_TO_MANY_H
