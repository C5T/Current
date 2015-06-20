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

#include "../../Bricks/cerealize/cerealize.h"

namespace blocks {
namespace persistence {

namespace impl {

template <template <class> class T_PERSISTENCE_LAYER, typename E>
class Impl final : public T_PERSISTENCE_LAYER<E> {
 private:
  typedef T_PERSISTENCE_LAYER<E> PERSISTENCE_LAYER;

 public:
  template <typename... ARGS>
  Impl(const std::function<E(const E&)> clone, ARGS&&... args)
      : PERSISTENCE_LAYER(std::forward<ARGS>(args)...), clone_(clone) {
    PERSISTENCE_LAYER::Replay([this](E&& e) { list_.push_back(std::move(e)); });
  }

  Impl(const Impl&) = delete;

  size_t Publish(const E& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    PERSISTENCE_LAYER::Publish(entry);
    list_.push_back(entry);
    return list_.size() - 1;
  }
  size_t Publish(E&& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    PERSISTENCE_LAYER::Publish(static_cast<const E&>(entry));
    list_.push_back(std::move(entry));
    return list_.size() - 1;
  }
  template <typename... ARGS>
  size_t Emplace(ARGS&&... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    list_.emplace_back(std::forward<ARGS>(args)...);
    PERSISTENCE_LAYER::Publish(list_.back());
    return list_.size() - 1;
  }

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
      blocks::CallReplayDone(f);
      replay_done = true;
    }

    bool notified_about_termination = false;
    while (true) {
      if (stop && !notified_about_termination) {
        notified_about_termination = true;
        if (blocks::CallTerminate(f)) {
          return;
        }
      }
      if (!current.at_end) {
        if (!blocks::DispatchEntryByConstReference(
                f, *current.iterator, current.index, current.total, clone_)) {
          break;
        }
        if (!replay_done && current.index + 1 >= size_at_start) {
          blocks::CallReplayDone(f);
          replay_done = true;
        }
      }
      Cursor next;
      do {
        if (stop && !notified_about_termination) {
          notified_about_termination = true;
          if (blocks::CallTerminate(f)) {
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

 private:
  std::list<E> list_;  // `std::list<>` does not invalidate iterators as new elements are added.
  std::mutex mutex_;
  const std::function<E(const E&)> clone_;
};

template <typename E>
struct MemoryOnlyImpl {
  void Replay(std::function<void(E&&)>) {}
  void Publish(const E&) {}
};

template <typename E>
class AppendToFileImpl {
 protected:
  AppendToFileImpl(const std::string& filename) : filename_(filename) {}

  void Replay(std::function<void(E&&)> push) {
    // TODO(dkorolev): Try/catch here?
    assert(!appender_);
    bricks::cerealize::CerealJSONFileParser<E> parser(filename_);
    while (parser.Next(push)) {
      ;
    }
    appender_ = make_unique<bricks::cerealize::CerealJSONFileAppender<E>>(filename_);
    assert(appender_);
  }

  void Publish(const E& e) { (*appender_) << e; }

 private:
  const std::string& filename_;
  std::unique_ptr<bricks::cerealize::CerealJSONFileAppender<E>> appender_;
};

}  // namespace blocks::persistence::impl

template <typename E>
using MemoryOnly = impl::Impl<impl::MemoryOnlyImpl, E>;

template <typename E>
using AppendToFile = impl::Impl<impl::AppendToFileImpl, E>;

}  // namespace persistence
}  // namespace blocks

#endif  // BLOCKS_PERSISTENCE_PERSISTENCE_H
