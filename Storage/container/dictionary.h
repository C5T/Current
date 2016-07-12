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

#ifndef CURRENT_STORAGE_CONTAINER_DICTIONARY_H
#define CURRENT_STORAGE_CONTAINER_DICTIONARY_H

#include "common.h"
#include "sfinae.h"

#include "../base.h"

#include "../../TypeSystem/optional.h"

namespace current {
namespace storage {
namespace container {

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT, template <typename...> class MAP>
class GenericDictionary {
 public:
  using key_t = sfinae::ENTRY_KEY_TYPE<T>;
  using map_t = MAP<key_t, T>;
  using rest_behavior_t = rest::behavior::Dictionary;

  GenericDictionary(MutationJournal& journal) : journal_(journal) {}

  bool Empty() const { return map_.empty(); }
  size_t Size() const { return map_.size(); }

  ImmutableOptional<T> operator[](sfinae::CF<key_t> key) const {
    const auto iterator = map_.find(key);
    if (iterator != map_.end()) {
      return ImmutableOptional<T>(FromBarePointer(), &iterator->second);
    } else {
      return nullptr;
    }
  }

  ImmutableOptional<std::chrono::microseconds> LastModified(sfinae::CF<key_t> key) const {
    const auto iterator = last_modified_.find(key);
    if (iterator != last_modified_.end()) {
      return ImmutableOptional<std::chrono::microseconds>(iterator->second);
    } else {
      return nullptr;
    }
  }

  void Add(const T& object) {
    const auto now = current::time::Now();
    const auto key = sfinae::GetKey(object);
    const auto map_iterator = map_.find(key);
    const auto lm_iterator = last_modified_.find(key);
    if (map_iterator != map_.end()) {
      const T& previous_object = map_iterator->second;
      assert(lm_iterator != last_modified_.end());
      const auto previous_timestamp = lm_iterator->second;
      journal_.LogMutation(UPDATE_EVENT(now, object),
                           [this, key, previous_object, previous_timestamp]() {
                             last_modified_[key] = previous_timestamp;
                             map_[key] = previous_object;
                           });
    } else {
      if (lm_iterator != last_modified_.end()) {
        const auto previous_timestamp = lm_iterator->second;
        journal_.LogMutation(UPDATE_EVENT(now, object),
                             [this, key, previous_timestamp]() {
                               last_modified_[key] = previous_timestamp;
                               map_.erase(key);
                             });
      } else {
        journal_.LogMutation(UPDATE_EVENT(now, object),
                             [this, key]() {
                               last_modified_.erase(key);
                               map_.erase(key);
                             });
      }
    }
    last_modified_[key] = now;
    map_[key] = object;
  }

  void Erase(sfinae::CF<key_t> key) {
    const auto now = current::time::Now();
    const auto map_iterator = map_.find(key);
    if (map_iterator != map_.end()) {
      const T& previous_object = map_iterator->second;
      const auto lm_iterator = last_modified_.find(key);
      assert(lm_iterator != last_modified_.end());
      const auto previous_timestamp = lm_iterator->second;
      journal_.LogMutation(DELETE_EVENT(now, previous_object),
                           [this, key, previous_object, previous_timestamp]() {
                             last_modified_[key] = previous_timestamp;
                             map_[key] = previous_object;
                           });
      last_modified_[key] = now;
      map_.erase(key);
    }
  }

  void operator()(const UPDATE_EVENT& e) {
    const auto key = sfinae::GetKey(e.data);
    last_modified_[key] = e.us;
    map_[key] = e.data;
  }
  void operator()(const DELETE_EVENT& e) {
    last_modified_[e.key] = e.us;
    map_.erase(e.key);
  }

  struct Iterator final {
    using iterator_t = typename map_t::const_iterator;
    iterator_t iterator;
    explicit Iterator(iterator_t iterator) : iterator(std::move(iterator)) {}
    void operator++() { ++iterator; }
    bool operator==(const Iterator& rhs) const { return iterator == rhs.iterator; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    sfinae::CF<key_t> key() const { return iterator->first; }
    const T& operator*() const { return iterator->second; }
    const T* operator->() const { return &iterator->second; }
  };

  Iterator begin() const { return Iterator(map_.cbegin()); }
  Iterator end() const { return Iterator(map_.cend()); }

 private:
  map_t map_;
  std::unordered_map<key_t, std::chrono::microseconds, CurrentHashFunction<key_t>> last_modified_;
  MutationJournal& journal_;
};

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using UnorderedDictionary = GenericDictionary<T, UPDATE_EVENT, DELETE_EVENT, Unordered>;

template <typename T, typename UPDATE_EVENT, typename DELETE_EVENT>
using OrderedDictionary = GenericDictionary<T, UPDATE_EVENT, DELETE_EVENT, Ordered>;

}  // namespace container

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::UnorderedDictionary<T, E1, E2>> {
  static const char* HumanReadableName() { return "UnorderedDictionary"; }
};

template <typename T, typename E1, typename E2>  // Entry, update event, delete event.
struct StorageFieldTypeSelector<container::OrderedDictionary<T, E1, E2>> {
  static const char* HumanReadableName() { return "OrderedDictionary"; }
};

}  // namespace storage
}  // namespace current

using current::storage::container::UnorderedDictionary;
using current::storage::container::OrderedDictionary;

#endif  // CURRENT_STORAGE_CONTAINER_DICTIONARY_H
