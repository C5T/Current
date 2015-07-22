/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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
#include <functional>
#include <mutex>
#include <thread>

#include "../SS/ss.h"

#include "../../Bricks/cerealize/json.h"
#include "../../Bricks/cerealize/cerealize.h"
#include "../../Bricks/util/clone.h"
#include "../../Bricks/util/waitable_terminate_signal.h"

// TODO(dkorolev): Either include our profiler or eliminate profiling altogether. Sync w/ Max.
#ifndef PROFILER_SCOPE
#define PROFILER_SCOPE(x)
#endif

namespace blocks {
namespace persistence {

namespace impl {

class ThreeStageMutex {
 public:
  struct TopLevelScopedLock {
    explicit TopLevelScopedLock(ThreeStageMutex& parent) : guard_(parent.stage1_) {}
    std::lock_guard<std::mutex> guard_;
  };

  struct ThreeStagesScopedLock {
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
      assert(stage_ == 3);
      parent_.stage3_.unlock();
    }
    int stage_ = 1;
    ThreeStageMutex& parent_;
  };

  struct InnerLevelScopedUniqueLock {
    explicit InnerLevelScopedUniqueLock(ThreeStageMutex& parent) : unique_guard_(parent.stage3_) {}
    std::unique_lock<std::mutex>& GetUniqueLock() { return unique_guard_; }
    std::unique_lock<std::mutex> unique_guard_;
  };

 private:
  std::mutex stage1_;  // Update internal container.
  std::mutex stage2_;  // Publish the newly saved entry to the persistence layer.
  std::mutex stage3_;  // Notify the listeners that a new entry is ready.
};

template <class PERSISTENCE_LAYER, typename ENTRY, class CLONER>
class Logic : bricks::WaitableTerminateSignalBulkNotifier {
 public:
  template <typename... EXTRA_PARAMS>
  explicit Logic(EXTRA_PARAMS&&... extra_params)
      : persistence_layer_(std::forward<EXTRA_PARAMS>(extra_params)...) {
    persistence_layer_.Replay([this](ENTRY&& e) {
      ListPushBackImpl(std::move(e));
    });
  }

  Logic(const Logic&) = delete;

  template <typename F>
  void SyncScanAllEntries(bricks::WaitableTerminateSignal& waitable_terminate_signal, F&& f) {
    struct Cursor {
      bool at_end = true;
      size_t index = 0u;
      size_t total = 0u;
      typename std::forward_list<ENTRY>::const_iterator iterator;
      static Cursor Next(const Cursor& current,
                         const std::forward_list<ENTRY>& exclusively_accessed_list,
                         const size_t& exclusively_accessed_list_size) {
        Cursor next;
        if (current.at_end) {
          next.iterator = exclusively_accessed_list.begin();
          next.index = 0u;
        } else {
          assert(current.iterator != exclusively_accessed_list.end());
          next.iterator = current.iterator;
          ++next.iterator;
          next.index = current.index + 1u;
        }
        next.total = exclusively_accessed_list_size;
        next.at_end = (next.iterator == exclusively_accessed_list.end());
        return next;
      }
    };
    Cursor current;

    const size_t size_at_start = [this]() {
      // LOCKED: Get the number of entries before sending them to the listener.
      PROFILER_SCOPE("SyncScanAllEntries() `size_at_start`");
      ThreeStageMutex::TopLevelScopedLock lock(three_stage_mutex_);
      return list_size_;
    }();
    bool replay_done = false;

    if (!size_at_start) {
      blocks::ss::CallReplayDone(f);
      replay_done = true;
    }

    bool notified_about_termination = false;
    while (true) {
      if (waitable_terminate_signal && !notified_about_termination) {
        notified_about_termination = true;
        if (blocks::ss::CallTerminate(f)) {
          return;
        }
      }
      if (!current.at_end) {
        // Only specify the `CLONER` template parameter, the rest are best to be inferred.
        if (!blocks::ss::DispatchEntryByConstReference<CLONER>(
                std::forward<F>(f), *current.iterator, current.index, current.total)) {
          break;
        }
        if (!replay_done && current.index + 1u >= size_at_start) {
          blocks::ss::CallReplayDone(f);
          replay_done = true;
        }
      }
      Cursor next;
      do {
        if (waitable_terminate_signal && !notified_about_termination) {
          notified_about_termination = true;
          if (blocks::ss::CallTerminate(f)) {
            return;
          }
        }
        next = [&current, this]() {
          // LOCKED: Move the cursor forward.
          PROFILER_SCOPE("SyncScanAllEntries(), advance cursor");
          ThreeStageMutex::TopLevelScopedLock lock(three_stage_mutex_);
          PROFILER_SCOPE("lock acquired");
          return Cursor::Next(current, list_, std::ref(list_size_));
        }();
        if (next.at_end) {
          // Wait until one of two events take place:
          // 1) The number of messages in the `list_` exceeds `next.total`.
          //    Note that this might happen between `next.total` was captured and now.
          // 2) The listener thread has been externally requested to terminate.
          [this, &next, &waitable_terminate_signal]() {
            // LOCKED: Wait for either new data to become available or for an external termination request.
            PROFILER_SCOPE("SyncScanAllEntries(), wait for new data");
            ThreeStageMutex::InnerLevelScopedUniqueLock unique_lock(three_stage_mutex_);
            PROFILER_SCOPE("lock acquired");
            bricks::WaitableTerminateSignalBulkNotifier::Scope scope(this, waitable_terminate_signal);
            waitable_terminate_signal.WaitUntil(unique_lock.GetUniqueLock(),
                                                [this, &next]() { return list_size_ > next.total; });
          }();
        }
      } while (next.at_end);
      current = next;
    }
  }

 protected:
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  size_t DoPublish(const ENTRY& entry) {
    PROFILER_SCOPE("DoPublish(const ENTRY&)");
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    {
      PROFILER_SCOPE("lock acquired");
      const size_t result = list_size_;
      {
        PROFILER_SCOPE("push_back");
        ListPushBackImpl(entry);
      }
      {
        PROFILER_SCOPE("promote lock 1->2");
        lock.AdvanceToStageTwo();
      }
      {
        PROFILER_SCOPE("publish");
        persistence_layer_.Publish(entry);
      }
      {
        PROFILER_SCOPE("promote lock 2->3");
        lock.AdvanceToStageThree();
      }
      {
        PROFILER_SCOPE("notify");
        NotifyAllOfExternalWaitableEvent();
      }
      return result;
    }
  }
  size_t DoPublish(ENTRY&& entry) {
    PROFILER_SCOPE("DoPublish(E&&)");
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    {
      PROFILER_SCOPE("lock acquired");
      const size_t result = list_size_;
      {
        PROFILER_SCOPE("push_back, std::move");
        ListPushBackImpl(std::move(entry));
      }
      const ENTRY& pushed_entry = *list_back_;
      {
        PROFILER_SCOPE("promote lock 1->2");
        lock.AdvanceToStageTwo();
      }
      {
        PROFILER_SCOPE("publish");
        persistence_layer_.Publish(pushed_entry);
      }
      {
        PROFILER_SCOPE("1->3");
        lock.AdvanceToStageThree();
      }
      {
        PROFILER_SCOPE("notify");
        NotifyAllOfExternalWaitableEvent();
      }
      return result;
    }
  }

  template <typename DERIVED_ENTRY>
  size_t DoPublishDerived(const DERIVED_ENTRY& entry) {
    static_assert(bricks::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, "");

    PROFILER_SCOPE("DoPublishDerived(const DERIVED_ENTRY&)");
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);

    {
      PROFILER_SCOPE("lock acquired");

      // `std::unique_ptr<DERIVED_ENTRY>` can be implicitly converted into `std::unique_ptr<ENTRY>`,
      // if `ENTRY` is the base class for `DERIVED_ENTRY`.
      // This requires the destructor of `BASE` to be virtual, which is the case for Current and Yoda.
      std::unique_ptr<DERIVED_ENTRY> copy(make_unique<DERIVED_ENTRY>());
      {
        PROFILER_SCOPE("clone");
        *copy = bricks::DefaultCloneFunction<DERIVED_ENTRY>()(entry);
      }
      const size_t result = list_size_;
      {
        PROFILER_SCOPE("push_back, std::move");
        ListPushBackImpl(std::move(copy));
      }
      const ENTRY& pushed_entry = *list_back_;
      {
        PROFILER_SCOPE("promote lock 1->2");
        lock.AdvanceToStageTwo();
      }
      {
        PROFILER_SCOPE("publish");
        persistence_layer_.Publish(pushed_entry);

        // A simple construction, commented out below, would require `DERIVED_ENTRY` to define
        // the copy constructor. Instead, we go with Current-friendly clone implementation above.
        // COMMENTED OUT: persistence_layer_.Publish(entry);
        // COMMENTED OUT: list_.push_back(std::move(make_unique<DERIVED_ENTRY>(entry)));

        // Another, semantically correct yet inefficient way, is to use JavaScript-style cloning.
        // COMMENTED OUT: persistence_layer_.Publish(entry);
        // COMMENTED OUT: list_.push_back(ParseJSON<ENTRY>(JSON(WithBaseType<typename
        // ENTRY::element_type>(entry))));
      }
      {
        PROFILER_SCOPE("promote lock 2->3");
        lock.AdvanceToStageThree();
      }

      {
        PROFILER_SCOPE("notify");
        NotifyAllOfExternalWaitableEvent();
      }
      return result;
    }
  }

  template <typename... ARGS>
  size_t DoEmplace(ARGS&&... args) {
    PROFILER_SCOPE("DoEmplace(...)");
    ThreeStageMutex::ThreeStagesScopedLock lock(three_stage_mutex_);
    {
      PROFILER_SCOPE("lock acquired");
      const size_t result = list_size_;
      {
        PROFILER_SCOPE("emplace_back");
        ListEmplaceBackImpl(std::forward<ARGS>(args)...);
      }
      const ENTRY& pushed_entry = *list_back_;
      {
        PROFILER_SCOPE("promote lock 1->2");
        lock.AdvanceToStageTwo();
      }
      {
        PROFILER_SCOPE("publish");
        persistence_layer_.Publish(pushed_entry);
      }
      {
        PROFILER_SCOPE("promote lock promote lock 2->3");
        lock.AdvanceToStageThree();
      }
      {
        PROFILER_SCOPE("notify");
        NotifyAllOfExternalWaitableEvent();
      }
      return result;
    }
  }

 private:
  // `std::forward_list<>` and its size + iterator management.
  template <typename E>
  void ListPushBackImpl(E&& e) {
    if (list_size_) {
      list_.insert_after(list_back_, std::forward<E>(e));
      ++list_back_;
    } else {
      list_.push_front(std::forward<E>(e));
      list_back_ = list_.begin();
    }
    ++list_size_;
  }
  template <typename... ARGS>
  void ListEmplaceBackImpl(ARGS&&... args) {
    if (list_size_) {
      list_.emplace_after(list_back_, std::forward<ARGS>(args)...);
      ++list_back_;
    } else {
      list_.emplace_front(std::forward<ARGS>(args)...);
      list_back_ = list_.begin();
    }
    ++list_size_;
  }

 private:
  static_assert(ss::IsEntryPublisher<PERSISTENCE_LAYER, ENTRY>::value, "");
  PERSISTENCE_LAYER persistence_layer_;

  // `std::forward_list<>` does not invalidate iterators as new elements are added.
  // Explicitly refrain from using an `std::list<>` due to its `.size()` complexity troubles on gcc/Linux.
  std::forward_list<ENTRY> list_;
  size_t list_size_ = 0;
  typename std::forward_list<ENTRY>::const_iterator list_back_;  // Only valid iff `list_size_` > 0.

  // To release the locks for the `std::forward_list<>` and the persistence layer ASAP.
  ThreeStageMutex three_stage_mutex_;
};

// The implementation of a "publisher into nowhere".
template <typename ENTRY, class CLONER>
struct DevNullPublisherImpl {
  void Replay(std::function<void(ENTRY&&)>) {}
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  size_t DoPublish(const ENTRY&) { return ++count_; }
  size_t DoPublish(ENTRY&&) { return ++count_; }
  template <typename DERIVED_ENTRY>
  size_t DoPublishDerived(const DERIVED_ENTRY&) {
    static_assert(bricks::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, "");
    return ++count_;
  }
  size_t count_ = 0u;
};

template <typename ENTRY, class CLONER>
using DevNullPublisher = ss::Publisher<DevNullPublisherImpl<ENTRY, CLONER>, ENTRY>;

// TODO(dkorolev): Move into Cerealize.
template <typename ENTRY, class CLONER>
struct AppendToFilePublisherImpl {
  AppendToFilePublisherImpl() = delete;
  AppendToFilePublisherImpl(const AppendToFilePublisherImpl&) = delete;
  explicit AppendToFilePublisherImpl(const std::string& filename) : filename_(filename) {}

  void Replay(std::function<void(ENTRY&&)> push) {
    // TODO(dkorolev): Try/catch here?
    assert(!appender_);
    bricks::cerealize::CerealJSONFileParser<ENTRY> parser(filename_);
    while (parser.Next(push)) {
      ++count_;
    }
    appender_ = make_unique<bricks::cerealize::CerealJSONFileAppender<ENTRY, CLONER>>(filename_);
    assert(appender_);
  }

  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  size_t DoPublish(const ENTRY& entry) {
    (*appender_) << entry;
    return ++count_;
  }
  size_t DoPublish(ENTRY&& entry) {
    (*appender_) << entry;
    return ++count_;
  }

  template <typename DERIVED_ENTRY>
  size_t DoPublishDerived(const DERIVED_ENTRY& e) {
    static_assert(bricks::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, "");
    (*appender_) << WithBaseType<ENTRY>(e);
    return ++count_;
  }

 private:
  const std::string filename_;
  std::unique_ptr<bricks::cerealize::CerealJSONFileAppender<ENTRY, CLONER>> appender_;
  size_t count_ = 0u;
};

template <typename ENTRY, class CLONER>
using AppendToFilePublisher = ss::Publisher<impl::AppendToFilePublisherImpl<ENTRY, CLONER>, ENTRY>;

}  // namespace blocks::persistence::impl

template <typename ENTRY, class CLONER = bricks::DefaultCloner>
using MemoryOnly = ss::Publisher<impl::Logic<impl::DevNullPublisher<ENTRY, CLONER>, ENTRY, CLONER>, ENTRY>;

template <typename ENTRY, class CLONER = bricks::DefaultCloner>
using AppendToFile =
    ss::Publisher<impl::Logic<impl::AppendToFilePublisher<ENTRY, CLONER>, ENTRY, CLONER>, ENTRY>;

}  // namespace blocks::persistence
}  // namespace blocks

#endif  // BLOCKS_PERSISTENCE_PERSISTENCE_H
