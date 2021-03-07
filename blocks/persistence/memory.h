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
// Stores all entries in an `std::deque<std::pair<std::chrono::microseconds, ENTRY>>`,
// and accesses this "deque" by indexes from under a mutex when iterating over the entries.
// Iterators never outlive the persister.

// NOTE(dkorolev): I took the liberty to use a dedicated `std::mutex` in `MemoryPersister`.
// Rationale: While it's possible to optimize mutex usage for thread-safety, I've concluded
// that's what `FilePersister` does. The `MemoryPersister` one is just for safe unit-testing;
// its primary goal is to help find issues unrelated to the persister itself. Thus,
// rather than making it mutex-efficient, I'd rather make it mutex-bulletproof.

#ifndef BLOCKS_PERSISTENCE_MEMORY_H
#define BLOCKS_PERSISTENCE_MEMORY_H

#include <deque>
#include <functional>
#include <mutex>

#include "exceptions.h"

#include "../ss/persister.h"
#include "../ss/signature.h"

#include "../../bricks/sync/locks.h"
#include "../../bricks/sync/owned_borrowed.h"
#include "../../bricks/time/chrono.h"

namespace current {
namespace persistence {

namespace impl {

template <typename ENTRY>
class MemoryPersister {
 private:
  struct Container {
    using entry_t = std::pair<std::chrono::microseconds, ENTRY>;

    mutable std::mutex memory_persister_container_mutex_;
    std::deque<entry_t> entries_;
    std::chrono::microseconds head_ = std::chrono::microseconds(-1);

    explicit Container(std::mutex& unused_publish_mutex_ref) {
      static_cast<void>(unused_publish_mutex_ref);  // `MemoryPersister` is not mutex-efficient. -- D.K.
    }
  };

 public:
  MemoryPersister(std::mutex& unused_publish_mutex_ref, const ss::StreamNamespaceName&)
      : container_(MakeOwned<Container>(unused_publish_mutex_ref)) {}

  class Iterator {
   public:
    Iterator(Borrowed<Container> container, uint64_t i) : container_(std::move(container)), i_(i) {}

    struct Entry {
      const idxts_t idx_ts;
      const ENTRY& entry;

      Entry() = delete;
      Entry(uint64_t index, const std::pair<std::chrono::microseconds, ENTRY>& input)
          : idx_ts(index, input.first), entry(input.second) {}
    };

    Iterator() = delete;
    Iterator(const Iterator&) = delete;
    Iterator(Iterator&& rhs) : container_(std::move(rhs.container_)), i_(rhs.i_) {}
    Iterator& operator=(const Iterator&) = delete;
    Iterator& operator=(Iterator&&) = default;

    Entry operator*() const {
      std::lock_guard<std::mutex> lock(container_->memory_persister_container_mutex_);
      return Entry(i_, container_->entries_[static_cast<size_t>(i_)]);
    }
    Iterator& operator++() {
      ++i_;
      return *this;
    }
    bool operator==(const Iterator& rhs) const { return i_ == rhs.i_; }
    bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }
    operator bool() const { return container_; }

   private:
    mutable Borrowed<Container> container_;  // DIMA: Why `mutable`?
    uint64_t i_;
  };

  class IteratorUnsafe {
   public:
    IteratorUnsafe(Borrowed<Container> container, uint64_t i) : container_(std::move(container)), i_(i) {}

    std::string operator*() const {
      std::lock_guard<std::mutex> lock(container_->memory_persister_container_mutex_);
      const auto& entry = container_->entries_[static_cast<size_t>(i_)];
      return JSON(idxts_t(i_, entry.first)) + '\t' + JSON(entry.second);
    }
    IteratorUnsafe& operator++() {
      ++i_;
      return *this;
    }
    bool operator==(const IteratorUnsafe& rhs) const { return i_ == rhs.i_; }
    bool operator!=(const IteratorUnsafe& rhs) const { return !operator==(rhs); }
    operator bool() const { return container_; }

   private:
    mutable Borrowed<Container> container_;
    uint64_t i_;
  };

  template <class ITERATOR>
  class IterableRangeImpl {
   public:
    IterableRangeImpl(Borrowed<Container> container, uint64_t begin, uint64_t end)
        : container_(std::move(container)), begin_(begin), end_(end) {}

    IterableRangeImpl(IterableRangeImpl&& rhs)
        : container_(std::move(rhs.container_)), begin_(rhs.begin_), end_(rhs.end_) {}

    ITERATOR begin() const { return ITERATOR(container_, begin_); }
    ITERATOR end() const { return ITERATOR(container_, end_); }
    operator bool() const { return container_; }

   private:
    const Borrowed<Container> container_;
    const uint64_t begin_;
    const uint64_t end_;
  };

  template <current::locks::MutexLockStatus MLS, typename E, typename TIMESTAMP>
  idxts_t PersisterPublishImpl(E&& entry, const TIMESTAMP user_timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    const auto head = container_->head_;
    const auto timestamp = current::time::TimestampAsMicroseconds(user_timestamp);
    if (!(timestamp > head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), timestamp));
    }
    const auto index = static_cast<uint64_t>(container_->entries_.size());
    container_->entries_.emplace_back(timestamp, std::forward<E>(entry));
    container_->head_ = timestamp;
    CURRENT_ASSERT(container_->head_ >= container_->entries_.back().first);
    return idxts_t(index, timestamp);
  }

  template <current::locks::MutexLockStatus MLS>
  idxts_t PersisterPublishUnsafeImpl(const std::string& raw_log_line) {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    const auto head = container_->head_;
    const auto tab_pos = raw_log_line.find('\t');
    if (tab_pos == std::string::npos) {
      CURRENT_THROW(MalformedEntryException(raw_log_line));
    }
    const auto idxts = ParseJSON<idxts_t>(raw_log_line.substr(0, tab_pos));
    const auto expected_index = static_cast<uint64_t>(container_->entries_.size());
    if (idxts.index != expected_index) {
      CURRENT_THROW(UnsafePublishBadIndexTimestampException(expected_index, idxts.index));
    }
    if (!(idxts.us > head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), idxts.us));
    }
    container_->entries_.emplace_back(idxts.us, ParseJSON<ENTRY>(raw_log_line.substr(tab_pos + 1)));
    container_->head_ = idxts.us;
    CURRENT_ASSERT(container_->head_ >= container_->entries_.back().first);
    return idxts;
  }

  template <current::locks::MutexLockStatus MLS, typename TIMESTAMP>
  void PersisterUpdateHeadImpl(const TIMESTAMP user_timestamp) {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);

    const auto timestamp = current::time::TimestampAsMicroseconds(user_timestamp);
    const auto head = container_->head_;
    if (!(timestamp > head)) {
      CURRENT_THROW(ss::InconsistentTimestampException(head + std::chrono::microseconds(1), timestamp));
    }
    container_->head_ = timestamp;
  }

  template <current::locks::MutexLockStatus MLS>
  bool PersisterEmptyImpl() const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    return container_->entries_.empty();
  }

  template <current::locks::MutexLockStatus MLS>
  uint64_t PersisterSizeImpl() const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    return static_cast<uint64_t>(container_->entries_.size());
  }

  template <current::locks::MutexLockStatus MLS>
  idxts_t PersisterLastPublishedIndexAndTimestampImpl() const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    if (!container_->entries_.empty()) {
      CURRENT_ASSERT(container_->head_ >= container_->entries_.back().first);
      return idxts_t(container_->entries_.size() - 1, container_->entries_.back().first);
    } else {
      CURRENT_THROW(NoEntriesPublishedYet());
    }
  }

  template <current::locks::MutexLockStatus MLS>
  head_optidxts_t PersisterHeadAndLastPublishedIndexAndTimestampImpl() const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    if (!container_->entries_.empty()) {
      CURRENT_ASSERT(container_->head_ >= container_->entries_.back().first);
      return head_optidxts_t(container_->head_, container_->entries_.size() - 1, container_->entries_.back().first);
    } else {
      return head_optidxts_t(container_->head_);
    }
  }

  template <current::locks::MutexLockStatus MLS>
  std::chrono::microseconds PersisterCurrentHeadImpl() const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    return container_->head_;
  }

  template <current::locks::MutexLockStatus MLS>
  std::pair<uint64_t, uint64_t> PersisterIndexRangeByTimestampRangeImpl(std::chrono::microseconds from,
                                                                        std::chrono::microseconds till) const {
    current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
    std::pair<uint64_t, uint64_t> result{static_cast<uint64_t>(-1), static_cast<uint64_t>(-1)};
    const auto begin_it =
        std::lower_bound(container_->entries_.begin(),
                         container_->entries_.end(),
                         from,
                         [](const typename Container::entry_t& e, std::chrono::microseconds t) { return e.first < t; });
    if (begin_it != container_->entries_.end()) {
      result.first = std::distance(container_->entries_.begin(), begin_it);
    }
    if (till.count() > 0) {
      const auto end_it = std::upper_bound(
          container_->entries_.begin(),
          container_->entries_.end(),
          till,
          [](std::chrono::microseconds t, const typename Container::entry_t& e) { return t < e.first; });
      if (end_it != container_->entries_.end()) {
        result.second = std::distance(container_->entries_.begin(), end_it);
      }
    }
    return result;
  }

  using IterableRange = IterableRangeImpl<Iterator>;
  using IterableRangeUnsafe = IterableRangeImpl<IteratorUnsafe>;

  template <current::locks::MutexLockStatus MLS>
  IterableRange PersisterIterate(uint64_t begin, uint64_t end) const {
    return PersisterIterateImpl<MLS, IterableRange>(begin, end);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRangeUnsafe PersisterIterateUnsafe(uint64_t begin, uint64_t end) const {
    return PersisterIterateImpl<MLS, IterableRangeUnsafe>(begin, end);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRange PersisterIterate(std::chrono::microseconds from, std::chrono::microseconds till) const {
    return PersisterIterateImpl<MLS, IterableRange>(from, till);
  }

  template <current::locks::MutexLockStatus MLS>
  IterableRangeUnsafe PersisterIterateUnsafe(std::chrono::microseconds from, std::chrono::microseconds till) const {
    return PersisterIterateImpl<MLS, IterableRangeUnsafe>(from, till);
  }

 private:
  template <current::locks::MutexLockStatus MLS, typename ITERABLE>
  ITERABLE PersisterIterateImpl(uint64_t begin, uint64_t end) const {
    const uint64_t size = [this]() {
      current::locks::SmartMutexLockGuard<MLS> lock(container_->memory_persister_container_mutex_);
      return static_cast<uint64_t>(container_->entries_.size());
    }();

    if (end == static_cast<uint64_t>(-1)) {
      end = size;
    }

    if (end > size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin == end) {
      return ITERABLE(container_, 0, 0);
    }
    if (begin >= size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (end < begin) {
      CURRENT_THROW(InvalidIterableRangeException());
    }

    return ITERABLE(container_, begin, end);
  }

  template <current::locks::MutexLockStatus MLS, typename ITERABLE>
  ITERABLE PersisterIterateImpl(std::chrono::microseconds from, std::chrono::microseconds till) const {
    if (till.count() > 0 && till < from) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    const auto index_range = PersisterIndexRangeByTimestampRangeImpl<MLS>(from, till);
    if (index_range.first != static_cast<uint64_t>(-1)) {
      return PersisterIterateImpl<MLS, ITERABLE>(index_range.first, index_range.second);
    } else {  // No entries found in the given range.
      return ITERABLE(container_, 0, 0);
    }
  }

 private:
  Owned<Container> container_;  // `Owned`, as iterators borrow it.
};

}  // namespace current::persistence::impl

template <typename ENTRY>
using Memory = ss::EntryPersister<impl::MemoryPersister<ENTRY>, ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_MEMORY_H
