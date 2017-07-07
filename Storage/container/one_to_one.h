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

#ifndef CURRENT_STORAGE_CONTAINER_ONE_TO_ONE_H
#define CURRENT_STORAGE_CONTAINER_ONE_TO_ONE_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../TypeSystem/optional.h"
#include "../../Bricks/util/comparators.h"  // For `GenericHashFunction`.
#include "../../Bricks/util/iterator.h"     // For `GenericMapIterator` and `GenericMapAccessor`.

namespace current {
namespace storage {
namespace container {

template <typename T,
          typename UPDATE_EVENT,
          typename DELETE_EVENT,
          template <typename...> class ROW_MAP,
          template <typename...> class COL_MAP>
class GenericOneToOne {
 public:
  using entry_t = T;
  using row_t = sfinae::entry_row_t<T>;
  using col_t = sfinae::entry_col_t<T>;
  using key_t = std::pair<row_t, col_t>;
  using elements_map_t = std::unordered_map<key_t, std::unique_ptr<T>, GenericHashFunction<key_t>>;
  using forward_map_t = ROW_MAP<row_t, const T*>;
  using transposed_map_t = COL_MAP<col_t, const T*>;
  using semantics_t = storage::semantics::OneToOne;

  GenericOneToOne(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  // Adds specified object and overwrites existing one if it has the same row and col.
  // Removes all other existing objects with the same row or col.
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
      const auto cit_row = forward_.find(row);
      const auto cit_col = transposed_.find(col);
      const bool row_occupied = (cit_row != forward_.end());
      const bool col_occupied = (cit_col != transposed_.end());
      if (row_occupied && col_occupied) {
        const T& conflicting_object_same_row = *(cit_row->second);
        const T& conflicting_object_same_col = *(cit_col->second);
        const auto key_same_row = std::make_pair(row, sfinae::GetCol(conflicting_object_same_row));
        const auto lm_same_row_cit = last_modified_.find(key_same_row);
        CURRENT_ASSERT(lm_same_row_cit != last_modified_.end());
        const auto timestamp_same_row = lm_same_row_cit->second;
        const auto key_same_col = std::make_pair(sfinae::GetRow(conflicting_object_same_col), col);
        const auto lm_same_col_cit = last_modified_.find(key_same_col);
        CURRENT_ASSERT(lm_same_col_cit != last_modified_.end());
        const auto timestamp_same_col = lm_same_col_cit->second;
        journal_.LogMutation(DELETE_EVENT(now, conflicting_object_same_row),
                             [this, key_same_row, conflicting_object_same_row, timestamp_same_row]() {
                               DoUpdateWithLastModified(timestamp_same_row, key_same_row, conflicting_object_same_row);
                             });
        DoEraseWithLastModified(now, key_same_row);
        now = current::time::Now();
        journal_.LogMutation(DELETE_EVENT(now, conflicting_object_same_col),
                             [this, key_same_col, conflicting_object_same_col, timestamp_same_col]() {
                               DoUpdateWithLastModified(timestamp_same_col, key_same_col, conflicting_object_same_col);
                             });
        DoEraseWithLastModified(now, key_same_col);
        now = current::time::Now();
      } else if (row_occupied || col_occupied) {
        const T& conflicting_object = row_occupied ? *(cit_row->second) : *(cit_col->second);
        const auto conflicting_object_key =
            std::make_pair(sfinae::GetRow(conflicting_object), sfinae::GetCol(conflicting_object));
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

  void EraseRow(sfinae::CF<row_t> row) {
    const auto now = current::time::Now();
    const auto forward_cit = forward_.find(row);
    if (forward_cit != forward_.end()) {
      const T& previous_object = *(forward_cit->second);
      const auto key = std::make_pair(row, sfinae::GetCol(previous_object));
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

  void EraseCol(sfinae::CF<col_t> col) {
    const auto now = current::time::Now();
    const auto transposed_cit = transposed_.find(col);
    if (transposed_cit != transposed_.end()) {
      const T& previous_object = *(transposed_cit->second);
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
  ImmutableOptional<T> GetEntryFromRow(sfinae::CF<row_t> row) const {
    const auto cit = forward_.find(row);
    if (cit != forward_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), cit->second);
    } else {
      return nullptr;
    }
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

  bool DoesNotConflict(const key_t& key) const {
    return forward_.find(key.first) == forward_.end() && transposed_.find(key.second) == transposed_.end();
  }
  bool DoesNotConflict(sfinae::CF<row_t> row, sfinae::CF<col_t> col) const {
    return DoesNotConflict(std::make_pair(row, col));
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto row = sfinae::GetRow(e.data);
    const auto col = sfinae::GetCol(e.data);
    DoUpdateWithLastModified(e.us, std::make_pair(row, col), e.data);
  }
  void operator()(const DELETE_EVENT& e) { DoEraseWithLastModified(e.us, std::make_pair(e.key.first, e.key.second)); }

  using rows_outer_accessor_t = GenericMapAccessor<forward_map_t>;
  rows_outer_accessor_t Rows() const { return GenericMapAccessor<forward_map_t>(forward_); }

  using cols_outer_accessor_t = GenericMapAccessor<transposed_map_t>;
  cols_outer_accessor_t Cols() const { return GenericMapAccessor<transposed_map_t>(transposed_); }

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
    forward_[key.first] = placeholder.get();
    transposed_[key.second] = placeholder.get();
  }

  void DoEraseWithoutTouchingLastModified(const key_t& key) {
    forward_.erase(key.first);
    transposed_.erase(key.second);
    map_.erase(key);
  }

  void DoEraseWithLastModified(std::chrono::microseconds us, const key_t& key) {
    last_modified_[key] = us;
    DoEraseWithoutTouchingLastModified(key);
  }

  elements_map_t map_;
  forward_map_t forward_;
  transposed_map_t transposed_;
  std::unordered_map<key_t, std::chrono::microseconds, GenericHashFunction<key_t>> last_modified_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedOneToUnorderedOne = GenericOneToOne<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedOneToOrderedOne = GenericOneToOne<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedOneToOrderedOne = GenericOneToOne<T, UPDATE_EVENT, DELETE_EVENT, Unordered, Ordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedOneToUnorderedOne = GenericOneToOne<T, UPDATE_EVENT, DELETE_EVENT, Ordered, Unordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOneToUnorderedOne<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOneToUnorderedOne"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOneToOrderedOne<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOneToOrderedOne"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedOneToOrderedOne<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedOneToOrderedOne"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedOneToUnorderedOne<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedOneToUnorderedOne"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedOneToUnorderedOne;
using current::storage::container::OrderedOneToOrderedOne;
using current::storage::container::UnorderedOneToOrderedOne;
using current::storage::container::OrderedOneToUnorderedOne;

#endif  // CURRENT_STORAGE_CONTAINER_ONE_TO_ONE_H
