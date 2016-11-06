/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
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

// A simple, reference, implementation of an in-memory persister.
// Store all entries in an `std::vector<std::pair<std::chrono::microseconds, ENTRY>>`,
// and accesses this vector by indexes from under a mutex when iterating over the entries.
// Iterators never outlive the persister.

#ifndef BLOCKS_PERSISTENCE_MEMORY_H
#define BLOCKS_PERSISTENCE_MEMORY_H

#include <deque>
#include <functional>
#include <mutex>

#include "exceptions.h"

#include "../SS/persister.h"

#include "../../Bricks/time/chrono.h"
#include "../../Bricks/sync/scope_owned.h"

namespace current {
namespace persistence {

namespace impl {

template <typename ENTRY>
class MemoryPersister {
 private:
  struct Container {
    using entry_t = std::pair<std::chrono::microseconds, ENTRY>;
    std::mutex mutex;
    std::deque<entry_t> entries;
    std::chrono::microseconds head = std::chrono::microseconds(-1);
  };

 public:
  MemoryPersister() : container_() {}

  class IterableRange {
   public:
    explicit IterableRange(ScopeOwned<Container>& container, uint64_t begin, uint64_t end)
        : container_(container, [this]() { valid_ = false; }), begin_(begin), end_(end) {}

    struct Entry {
      const idxts_t idx_ts;
      const ENTRY& entry;

      Entry() = delete;
      Entry(uint64_t index, const std::pair<std::chrono::microseconds, ENTRY>& input)
          : idx_ts(index, input.first), entry(input.second) {}
    };

    class Iterator {
     public:
      Iterator(ScopeOwned<Container>& container, uint64_t i)
          : container_(container, [this]() { valid_ = false; }), i_(i) {}

      Entry operator*() const {
        if (!valid_) {
          CURRENT_THROW(PersistenceMemoryBlockNoLongerAvailable());
        }
        std::lock_guard<std::mutex> lock(container_->mutex);
        return Entry(i_, container_->entries[i_]);
      }
      void operator++() {
        if (!valid_) {
          CURRENT_THROW(PersistenceMemoryBlockNoLongerAvailable());
        }
        ++i_;
      }
      bool operator==(const Iterator& rhs) const { return i_ == rhs.i_; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
      operator bool() const { return valid_; }

     private:
      mutable ScopeOwnedBySomeoneElse<Container> container_;
      bool valid_ = true;
      uint64_t i_;
    };

    Iterator begin() const {
      if (!valid_) {
        CURRENT_THROW(PersistenceMemoryBlockNoLongerAvailable());
      }
      return Iterator(container_, begin_);
    }
    Iterator end() const {
      if (!valid_) {
        CURRENT_THROW(PersistenceMemoryBlockNoLongerAvailable());
      }
      return Iterator(container_, end_);
    }
    operator bool() const { return valid_; }

   private:
    mutable ScopeOwnedBySomeoneElse<Container> container_;
    bool valid_ = true;
    const uint64_t begin_;
    const uint64_t end_;
  };

  template <typename E>
  idxts_t DoPublish(E&& entry, const std::chrono::microseconds us) {
    std::lock_guard<std::mutex> lock(container_->mutex);
    const auto head = container_->head;
    const auto timestamp = us.count() >= 0 ? us : current::time::Now();
    if (!(timestamp > head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), timestamp));
    }
    const auto index = static_cast<uint64_t>(container_->entries.size());
    container_->entries.emplace_back(timestamp, std::forward<E>(entry));
    container_->head = timestamp;
    return idxts_t(index, timestamp);
  }

  void DoUpdateHead(const std::chrono::microseconds us) {
    std::lock_guard<std::mutex> lock(container_->mutex);
    const auto timestamp = us.count() >= 0 ? us : current::time::Now();
    const auto head = container_->head;
    if (!(timestamp > head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), timestamp));
    }
    container_->head = timestamp;
  }

  bool Empty() const noexcept {
    std::lock_guard<std::mutex> lock(container_->mutex);
    return container_->entries.empty();
  }

  uint64_t Size() const noexcept {
    std::lock_guard<std::mutex> lock(container_->mutex);
    return static_cast<uint64_t>(container_->entries.size());
  }

  idxts_t LastPublishedIndexAndTimestamp() const {
    std::lock_guard<std::mutex> lock(container_->mutex);
    if (!container_->entries.empty()) {
      return idxts_t(container_->entries.size() - 1, container_->entries.back().first);
    } else {
      CURRENT_THROW(NoEntriesPublishedYet());
    }
  }

  head_optidxts_t HeadAndLastPublishedIndexAndTimestamp() const noexcept {
    std::lock_guard<std::mutex> lock(container_->mutex);
    if (!container_->entries.empty()) {
      return head_optidxts_t(container_->head, container_->entries.size() - 1, container_->entries.back().first);
    } else {
      return head_optidxts_t(container_->head);
    }
  }

  std::chrono::microseconds CurrentHead() const noexcept {
    std::lock_guard<std::mutex> lock(container_->mutex);
    return container_->head;
  }

  std::pair<uint64_t, uint64_t> IndexRangeByTimestampRange(std::chrono::microseconds from,
                                                           std::chrono::microseconds till) const {
    std::pair<uint64_t, uint64_t> result{static_cast<uint64_t>(-1), static_cast<uint64_t>(-1)};
    std::lock_guard<std::mutex> lock(container_->mutex);
    const auto begin_it =
        std::lower_bound(container_->entries.begin(),
                         container_->entries.end(),
                         from,
                         [](const typename Container::entry_t& e, std::chrono::microseconds t) { return e.first < t; });
    if (begin_it != container_->entries.end()) {
      result.first = std::distance(container_->entries.begin(), begin_it);
    }
    if (till.count() > 0) {
      const auto end_it = std::upper_bound(
          container_->entries.begin(),
          container_->entries.end(),
          till,
          [](std::chrono::microseconds t, const typename Container::entry_t& e) { return t < e.first; });
      if (end_it != container_->entries.end()) {
        result.second = std::distance(container_->entries.begin(), end_it);
      }
    }
    return result;
  }

  IterableRange Iterate(uint64_t begin, uint64_t end) const {
    const uint64_t size = [this]() {
      std::lock_guard<std::mutex> lock(container_->mutex);
      return static_cast<uint64_t>(container_->entries.size());
    }();

    if (end == static_cast<uint64_t>(-1)) {
      end = size;
    }

    if (end > size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin == end) {
      return IterableRange(container_, 0, 0);
    }
    if (begin >= size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (end < begin) {
      CURRENT_THROW(InvalidIterableRangeException());
    }

    return IterableRange(container_, begin, end);
  }

  IterableRange Iterate(std::chrono::microseconds from, std::chrono::microseconds till) const {
    if (till.count() > 0 && till < from) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    const auto index_range = IndexRangeByTimestampRange(from, till);
    if (index_range.first != static_cast<uint64_t>(-1)) {
      return Iterate(index_range.first, index_range.second);
    } else {  // No entries found in the given range.
      return IterableRange(container_, 0, 0);
    }
  }

 private:
  mutable ScopeOwnedByMe<Container> container_;
};

}  // namespace current::persistence::impl

template <typename ENTRY>
using Memory = ss::EntryPersister<impl::MemoryPersister<ENTRY>, ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_MEMORY_H
