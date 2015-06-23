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

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <thread>

#include "../SS/ss.h"

#include "../../Bricks/cerealize/json.h"
#include "../../Bricks/cerealize/cerealize.h"

namespace blocks {
namespace persistence {

namespace impl {

template <class PERSISTENCE_LAYER, typename E>
class Logic {
 public:
  template <typename... EXTRA_PARAMS>
  Logic(std::function<E(const E&)> clone, EXTRA_PARAMS&&... extra_params)
      : persistence_layer_(clone, std::forward<EXTRA_PARAMS>(extra_params)...), clone_(clone) {
    persistence_layer_.Replay([this](E&& e) { list_.push_back(std::move(e)); });
  }

  Logic(const Logic&) = delete;

  template <typename F>
  void SyncScanAllEntries(std::atomic_bool& stop, F&& f) {
    struct Cursor {
      bool at_end = true;
      size_t index = 0u;
      size_t total = 0u;
      typename std::list<E>::const_iterator iterator;
      static Cursor Next(const Cursor& current, const std::list<E>& exclusively_accessed_list) {
        Cursor next;
        if (current.at_end) {
          next.iterator = exclusively_accessed_list.begin();
          next.index = 0u;
        } else {
          assert(current.iterator != exclusively_accessed_list.end());
          next.iterator = current.iterator;
          ++next.iterator;
          next.index = current.index + 1;
        }
        next.total = exclusively_accessed_list.size();
        next.at_end = (next.iterator == exclusively_accessed_list.end());
        return next;
      }
    };
    Cursor current;

    const size_t size_at_start = [this]() {
      // LOCKED: Get the number of entries before sending them to the listener.
      std::lock_guard<std::mutex> lock(mutex_);
      return list_.size();
    }();
    bool replay_done = false;

    if (!size_at_start) {
      blocks::ss::CallReplayDone(f);
      replay_done = true;
    }

    bool notified_about_termination = false;
    while (true) {
      if (stop && !notified_about_termination) {
        notified_about_termination = true;
        if (blocks::ss::CallTerminate(f)) {
          return;
        }
      }
      if (!current.at_end) {
        if (!blocks::ss::DispatchEntryByConstReference(
                f, *current.iterator, current.index, current.total, clone_)) {
          break;
        }
        if (!replay_done && current.index + 1 >= size_at_start) {
          blocks::ss::CallReplayDone(f);
          replay_done = true;
        }
      }
      Cursor next;
      do {
        if (stop && !notified_about_termination) {
          notified_about_termination = true;
          if (blocks::ss::CallTerminate(f)) {
            return;
          }
        }
        next = [&current, this]() {
          // LOCKED: Move the cursor forward.
          std::lock_guard<std::mutex> lock(mutex_);
          return Cursor::Next(current, list_);
        }();
        if (next.at_end) {
          // TODO(dkorolev): Wait for { `stop` || new data available } in a smart way.
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
      } while (next.at_end);
      current = next;
    }
  }

 protected:
  size_t DoPublishByConstReference(const E& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    persistence_layer_.Publish(entry);
    list_.push_back(entry);
    return list_.size() - 1;
  }

  size_t DoPublishByRValueReference(E&& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    persistence_layer_.Publish(static_cast<const E&>(entry));
    list_.push_back(std::move(entry));
    return list_.size() - 1;
  }

  template <typename DERIVED_E>
  size_t DoPublishDerivedByConstReference(const DERIVED_E&) {
    static_assert(bricks::can_be_stored_in_unique_ptr<E, DERIVED_E>::value, "");
    // Do something smart -- pass the entry down w/o making a copy.
    // TODO(dkorolev): Will implement it when doing Yoda persistence. Just fail so far.
    static_cast<void>(*static_cast<const char*>(0));
    return static_cast<size_t>(-1);
  }

  template <typename... ARGS>
  size_t DoEmplace(ARGS&&... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    list_.emplace_back(std::forward<ARGS>(args)...);
    persistence_layer_.Publish(list_.back());
    return list_.size() - 1;
  }

 private:
  static_assert(ss::IsEntryPublisher<PERSISTENCE_LAYER, E>::value, "");
  PERSISTENCE_LAYER persistence_layer_;

  std::list<E> list_;  // `std::list<>` does not invalidate iterators as new elements are added.
  std::mutex mutex_;
  const std::function<E(const E&)> clone_;
};

// We keep the `clone` param part of the constructor signature of persistence layers.
// Right now persistence layers don't need to clone incoming entries, but we never know. -- D.K.

// The implementation of a "publisher into nowhere".
template <typename E>
struct DevNullPublisherImpl {
  DevNullPublisherImpl() = delete;
  DevNullPublisherImpl(std::function<E(const E&)>) {}
  void Replay(std::function<void(E&&)>) {}
  size_t DoPublishByConstReference(const E&) { return ++count_; }
  size_t DoPublishByRValueReference(E&&) { return ++count_; }
  template <typename DERIVED_E>
  size_t DoPublishDerived(const DERIVED_E&) {
    static_assert(bricks::can_be_stored_in_unique_ptr<E, DERIVED_E>::value, "");
    return ++count_;
  }
  size_t count_ = 0u;
};

template <typename E>
using DevNullPublisher = ss::Publisher<DevNullPublisherImpl<E>, E>;

// TODO(dkorolev): Move into Cerealize.
template <typename E>
struct AppendToFilePublisherImpl {
  AppendToFilePublisherImpl(std::function<E(const E&)> clone, const std::string& filename)
      : clone_(clone), filename_(filename) {}

  void Replay(std::function<void(E&&)> push) {
    // TODO(dkorolev): Try/catch here?
    assert(!appender_);
    bricks::cerealize::CerealJSONFileParser<E> parser(filename_);
    while (parser.Next(push)) {
      ++count_;
    }
    appender_ = make_unique<bricks::cerealize::CerealJSONFileAppender<E>>(clone_, filename_);
    assert(appender_);
  }

  size_t DoPublishByConstReference(const E& e) {
    // TODO(dkorolev): Remove this debug output.
    std::cerr << "PUSLISHING " << JSON(e) << " INTO '" << filename_ << "'\n";
    (*appender_) << e;
    return ++count_;
  }

  size_t DoPublishByRValueReference(E&& e) { return DoPublishByConstReference(static_cast<const E&>(e)); }

  template <typename DERIVED_E>
  size_t DoPublishDerived(const DERIVED_E& e) {
    static_assert(bricks::can_be_stored_in_unique_ptr<E, DERIVED_E>::value, "");
    std::cerr << "PUSLISHING POLMORPHIC " << JSON(e) << " INTO '" << filename_ << "'\n";
    (*appender_) << WithBaseType<E>(e);
    return ++count_;
  }

 private:
  const std::function<E(const E&)> clone_;
  const std::string filename_;
  std::unique_ptr<bricks::cerealize::CerealJSONFileAppender<E>> appender_;
  size_t count_ = 0u;
};

template <typename E>
using AppendToFilePublisher = ss::Publisher<impl::AppendToFilePublisherImpl<E>, E>;

}  // namespace blocks::persistence::impl

template <typename E>
using MemoryOnly = ss::Publisher<impl::Logic<impl::DevNullPublisher<E>, E>, E>;

template <typename E>
using AppendToFile = ss::Publisher<impl::Logic<impl::AppendToFilePublisher<E>, E>, E>;

}  // namespace blocks::persistence
}  // namespace blocks

#endif  // BLOCKS_PERSISTENCE_PERSISTENCE_H
