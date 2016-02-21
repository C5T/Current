/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
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

#ifndef BLOCKS_PERSISTENCE_PERSISTENCE_H
#define BLOCKS_PERSISTENCE_PERSISTENCE_H

#include <cassert>
#include <chrono>
#include <forward_list>
#include <fstream>
#include <functional>
#include <mutex>
#include <thread>

#include "exceptions.h"

#include "../../TypeSystem/Serialization/json.h"

#include "../SS/ss.h"

#include "../../Bricks/util/waitable_terminate_signal.h"

namespace current {
namespace persistence {

namespace impl {
using IDX_TS = current::ss::IndexAndTimestamp;

class ThreeStageMutex final {
 public:
  struct ContainerScopedLock final {
    explicit ContainerScopedLock(ThreeStageMutex& parent) : guard_(parent.stage1_) {}
    std::lock_guard<std::mutex> guard_;
  };

  struct ThreeStagesScopedLock final {
    explicit ThreeStagesScopedLock(ThreeStageMutex& parent) : parent_(parent) { parent_.stage1_.lock(); }
    void AdvanceToStageTwo() {
      assert(stage_ == 1);
      parent_.stage2_.lock();
      parent_.stage1_.unlock();
      stage_ = 2;
    }
    void AdvanceToStageThree() {
      assert(stage_ == 2);
      parent_.stage3_.lock();
      parent_.stage2_.unlock();
      stage_ = 3;
    }
    ~ThreeStagesScopedLock() {
      if (stage_ == 1) {
        parent_.stage1_.unlock();
      } else if (stage_ == 2) {
        parent_.stage2_.unlock();
      } else {
        assert(stage_ == 3);
        parent_.stage3_.unlock();
      }
    }
    int stage_ = 1;
    ThreeStageMutex& parent_;
  };

  struct NotifiersScopedUniqueLock final {
    explicit NotifiersScopedUniqueLock(ThreeStageMutex& parent) : unique_guard_(parent.stage3_) {}
    std::unique_lock<std::mutex>& GetUniqueLock() { return unique_guard_; }
    std::unique_lock<std::mutex> unique_guard_;
  };

 private:
  std::mutex stage1_;  // Publish the newly saved entry to the persistence layer, get `IndexAndTimestamp`.
  std::mutex stage2_;  // Update internal container, keeping corresponding `IndexAndTimestamp`.
  std::mutex stage3_;  // Notify the listeners that a new entry is ready.
};

template <class PERSISTENCE_LAYER, typename ENTRY>
class Logic : current::WaitableTerminateSignalBulkNotifier {
  using T_INTERNAL_CONTAINER = std::forward_list<std::pair<IDX_TS, ENTRY>>;

 public:
  template <typename... EXTRA_PARAMS>
  explicit Logic(EXTRA_PARAMS&&... extra_params)
      : persistence_layer_(std::forward<EXTRA_PARAMS>(extra_params)...) {
    persistence_layer_.Replay([this](IDX_TS idx_ts, ENTRY&& e) { ListPushBackImpl(idx_ts, std::move(e)); });
  }

  Logic(const Logic&) = delete;

  size_t Size() const {
    ThreeStageMutex::ContainerScopedLock lock(three_stage_mutex_);
    return list_size_;
  }

  // TODO(dkorolev): Enable gracefully shutting down the iterable range by having iterators throw.
  class IterableRange {
   public:
    struct Entry {
      const IDX_TS idx_ts;
      const ENTRY& entry;

      Entry() = delete;
      explicit Entry(const std::pair<IDX_TS, ENTRY>& input) : idx_ts(input.first), entry(input.second) {}
    };

    class Iterator {
     public:
      explicit Iterator(const typename T_INTERNAL_CONTAINER::const_iterator cit) : cit_(cit) {}
      Entry operator*() const { return Entry(*cit_); }
      void operator++() { ++cit_; }
      bool operator==(const Iterator& rhs) const { return cit_ == rhs.cit_; }
      bool operator!=(const Iterator& rhs) const { return !operator==(rhs); }

     private:
      typename T_INTERNAL_CONTAINER::const_iterator cit_;
    };

    IterableRange(const T_INTERNAL_CONTAINER& container, size_t begin_index, size_t end_index) {
      typename T_INTERNAL_CONTAINER::const_iterator cit = container.begin();
      size_t i = 0u;
      while (i < begin_index) {
        ++cit;
        ++i;
      }
      begin_ = cit;
      while (i < end_index) {
        ++cit;
        ++i;
      }
      end_ = cit;
    }

    Iterator begin() const { return Iterator(begin_); }
    Iterator end() const { return Iterator(end_); }

   private:
    typename T_INTERNAL_CONTAINER::const_iterator begin_;
    typename T_INTERNAL_CONTAINER::const_iterator end_;
  };

  IterableRange Iterate(size_t begin_index = 0u, size_t end_index = static_cast<size_t>(-1)) const {
    ThreeStageMutex::ContainerScopedLock lock(three_stage_mutex_);
    const size_t size = list_size_;
    if (end_index == static_cast<size_t>(-1)) {
      end_index = size;
    }
    if (end_index > size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (begin_index == end_index) {
      return IterableRange(list_, 0, 0);
    }
    if (begin_index >= size) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    if (end_index < begin_index) {
      CURRENT_THROW(InvalidIterableRangeException());
    }
    return IterableRange(list_, begin_index, end_index);
  }

 protected:
  IDX_TS DoPublish(const ENTRY& entry) {
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    const auto idx_ts = persistence_layer_.Publish(entry);
    lock.AdvanceToStageTwo();
    ListPushBackImpl(idx_ts, entry);
    lock.AdvanceToStageThree();
    NotifyAllOfExternalWaitableEvent();
    return idx_ts;
  }
  IDX_TS DoPublish(ENTRY&& entry) {
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    const ENTRY& entry_cref = entry;
    const auto idx_ts = persistence_layer_.Publish(entry_cref);
    lock.AdvanceToStageTwo();
    ListPushBackImpl(idx_ts, std::move(entry));
    lock.AdvanceToStageThree();
    NotifyAllOfExternalWaitableEvent();
    return idx_ts;
  }

  template <typename... ARGS>
  IDX_TS DoEmplace(ARGS&&... args) {
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    ENTRY entry = ENTRY(std::forward<ARGS>(args)...);
    const ENTRY& entry_cref = entry;
    const auto idx_ts = persistence_layer_.Publish(entry_cref);
    lock.AdvanceToStageTwo();
    ListPushBackImpl(idx_ts, std::move(entry));
    lock.AdvanceToStageThree();
    NotifyAllOfExternalWaitableEvent();
    return idx_ts;
  }

  void DoPublishReplayed(const ENTRY& entry, IDX_TS idx_ts) {
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    persistence_layer_.PublishReplayed(entry, idx_ts);
    lock.AdvanceToStageTwo();
    ListPushBackImpl(idx_ts, entry);
    lock.AdvanceToStageThree();
    NotifyAllOfExternalWaitableEvent();
  }
  void DoPublishReplayed(ENTRY&& entry, IDX_TS idx_ts) {
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    const ENTRY& entry_cref = entry;
    persistence_layer_.PublishReplayed(entry_cref, idx_ts);
    lock.AdvanceToStageTwo();
    ListPushBackImpl(idx_ts, std::move(entry));
    lock.AdvanceToStageThree();
    NotifyAllOfExternalWaitableEvent();
  }

 private:
  // `std::forward_list<>` and its size + iterator management.
  template <typename E>
  void ListPushBackImpl(IDX_TS idx_ts, E&& e) {
    if (list_size_) {
      list_.insert_after(list_back_, std::make_pair(idx_ts, std::forward<E>(e)));
      ++list_back_;
    } else {
      list_.push_front(std::make_pair(idx_ts, std::forward<E>(e)));
      list_back_ = list_.begin();
    }
    ++list_size_;
  }

  IDX_TS GetLastIndexAndTimestamp() const {
    if (list_size_) {
      return list_back_->first;
    } else {
      return IDX_TS();
    }
  }

 private:
  static_assert(ss::IsStreamPublisher<PERSISTENCE_LAYER, ENTRY>::value, "");
  PERSISTENCE_LAYER persistence_layer_;

  // `std::forward_list<>` does not invalidate iterators as new elements are added.
  // Explicitly refrain from using an `std::list<>` due to its `.size()` complexity troubles on gcc/Linux.
  T_INTERNAL_CONTAINER list_;
  size_t list_size_ = 0;
  typename T_INTERNAL_CONTAINER::const_iterator list_back_;  // Only valid iff `list_size_` > 0.

  // To release the locks for the `std::forward_list<>` and the persistence layer ASAP.
  mutable ThreeStageMutex three_stage_mutex_;
};

// The implementation of a "publisher into nowhere".
template <typename ENTRY>
struct DevNullPublisherImpl {
  void Replay(std::function<void(IDX_TS, ENTRY&&)>) {}
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  IDX_TS DoPublish(const ENTRY&) { return IDX_TS(++count_, current::time::Now()); }
  IDX_TS DoPublish(ENTRY&&) { return IDX_TS(++count_, current::time::Now()); }

  template <typename E>
  void DoPublishReplayed(E&&, IDX_TS idx_ts) {
    if (idx_ts.index == count_ + 1) {
      ++count_;
    } else {
      CURRENT_THROW(InconsistentIndexException(count_ + 1, idx_ts.index));
    }
  }

  size_t count_ = 0u;
};

template <typename ENTRY>
using DevNullPublisher = ss::StreamPublisher<DevNullPublisherImpl<ENTRY>, ENTRY>;

template <typename ENTRY>
struct FilePersisterImpl {
  FilePersisterImpl() = delete;
  FilePersisterImpl(const FilePersisterImpl&) = delete;
  explicit FilePersisterImpl(const std::string& filename) : filename_(filename) {}

  void Replay(std::function<void(IDX_TS, ENTRY&&)> push) {
    assert(!appender_);
    std::ifstream fi(filename_);
    if (fi.good()) {
      std::string line;
      while (std::getline(fi, line)) {
        const size_t tab_pos = line.find('\t');
        if (tab_pos == std::string::npos) {
          CURRENT_THROW(MalformedEntryDuringReplayException(line));
        }
        IDX_TS idx_ts = ParseJSON<IDX_TS>(line.substr(0, tab_pos));
        // Indexes must be strictly continuous.
        if (idx_ts.index != last_idx_ts_.index + 1) {
          CURRENT_THROW(InconsistentIndexException(last_idx_ts_.index + 1, idx_ts.index));
        }
        // Timestamps must monotonically increase.
        if (idx_ts.us <= last_idx_ts_.us) {
          CURRENT_THROW(InconsistentTimestampException(last_idx_ts_.us, idx_ts.us));
        }
        push(idx_ts, std::move(ParseJSON<ENTRY>(line.substr(tab_pos + 1))));
        last_idx_ts_ = idx_ts;
      }
      appender_ = std::make_unique<std::ofstream>(filename_, std::ofstream::app);
    } else {
      appender_ = std::make_unique<std::ofstream>(filename_, std::ofstream::trunc);
    }
    assert(appender_);
    assert(appender_->good());
  }

  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  IDX_TS DoPublish(const ENTRY& entry) {
    const IDX_TS new_idx_ts(last_idx_ts_.index + 1u, current::time::Now());
    (*appender_) << JSON(new_idx_ts) << '\t' << JSON(entry) << std::endl;
    last_idx_ts_ = new_idx_ts;
    return new_idx_ts;
  }
  IDX_TS DoPublish(ENTRY&& entry) {
    const IDX_TS new_idx_ts(last_idx_ts_.index + 1u, current::time::Now());
    (*appender_) << JSON(new_idx_ts) << '\t' << JSON(entry) << std::endl;
    last_idx_ts_ = new_idx_ts;
    return new_idx_ts;
  }

  void DoPublishReplayed(const ENTRY& entry, IDX_TS idx_ts) {
    if (idx_ts.index != last_idx_ts_.index + 1u) {
      CURRENT_THROW(InconsistentIndexException(last_idx_ts_.index + 1u, idx_ts.index));
    }
    if (idx_ts.us <= last_idx_ts_.us) {
      CURRENT_THROW(InconsistentTimestampException(last_idx_ts_.us, idx_ts.us));
    }
    (*appender_) << JSON(idx_ts) << '\t' << JSON(entry) << std::endl;
    last_idx_ts_ = idx_ts;
  }
  void DoPublishReplayed(ENTRY&& entry, IDX_TS idx_ts) {
    if (idx_ts.index != last_idx_ts_.index + 1u) {
      CURRENT_THROW(InconsistentIndexException(last_idx_ts_.index + 1u, idx_ts.index));
    }
    if (idx_ts.us <= last_idx_ts_.us) {
      CURRENT_THROW(InconsistentTimestampException(last_idx_ts_.us, idx_ts.us));
    }
    (*appender_) << JSON(idx_ts) << '\t' << JSON(entry) << std::endl;
    last_idx_ts_ = idx_ts;
  }

 private:
  const std::string filename_;
  std::unique_ptr<std::ofstream> appender_;
  IDX_TS last_idx_ts_;
};

template <typename ENTRY>
using FilePersister = ss::StreamPublisher<impl::FilePersisterImpl<ENTRY>, ENTRY>;

}  // namespace current::persistence::impl

template <typename ENTRY>
using Memory = ss::StreamPublisher<impl::Logic<impl::DevNullPublisher<ENTRY>, ENTRY>, ENTRY>;

template <typename ENTRY>
using File = ss::StreamPublisher<impl::Logic<impl::FilePersister<ENTRY>, ENTRY>, ENTRY>;

// Enable legacy names for now. Confirmed Current compiles with the next four lines commented out. -- D.K.
template <typename ENTRY>
using MemoryOnly = Memory<ENTRY>;
template <typename ENTRY>
using AppendToFile = File<ENTRY>;

}  // namespace current::persistence
}  // namespace current

#endif  // BLOCKS_PERSISTENCE_PERSISTENCE_H
