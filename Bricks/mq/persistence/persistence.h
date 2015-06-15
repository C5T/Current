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

// TODO(dkorolev): Sync up with Max on code locations, names, and relative paths.
//                 I suggest we promote Yoda, MMQ (under a different name),
//                 and Persistence (under a different name) to the top level directory.

#ifndef BRICKS_MQ_INTERFACE_PERSISTENCE_H
#define BRICKS_MQ_INTERFACE_PERSISTENCE_H

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <list>
#include <mutex>
#include <thread>

#include "../interface/interface.h"

#include "../../util/clone.h"

namespace bricks {
namespace persistence {

template <typename E>
class MemoryOnly {
 public:
  MemoryOnly() : clone_(DefaultCloneFunction<E>()) {}
  MemoryOnly(const std::function<E(const E&)> clone) : clone_(clone) {}

  MemoryOnly(const MemoryOnly&) = delete;

  size_t Publish(E&& entry) {
    std::lock_guard<std::mutex> lock(mutex_);
    list_.push_back(std::forward<E>(entry));
    return list_.size() - 1;
  }
  template <typename... ARGS>
  size_t Emplace(ARGS&&... args) {
    std::lock_guard<std::mutex> lock(mutex_);
    list_.emplace_back(std::forward<ARGS>(args)...);
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
      // LOCKED: Move the cursor forward.
      std::lock_guard<std::mutex> lock(mutex_);
      return list_.size();
    }();
    bool replay_done = false;

    if (!size_at_start) {
      mq::CallReplayDone(f);
      replay_done = true;
    }

    bool notified_about_termination = false;
    while (true) {
      if (stop && !notified_about_termination) {
        notified_about_termination = true;
        if (mq::CallTerminate(f)) {
          return;
        }
      }
      if (!current.at_end) {
        if (!mq::DispatchEntryByConstReference(f, *current.iterator, current.index, current.total, clone_)) {
          break;
        }
        if (!replay_done && current.index + 1 >= size_at_start) {
          mq::CallReplayDone(f);
          replay_done = true;
        }
      }
      Cursor next;
      do {
        if (stop && !notified_about_termination) {
          notified_about_termination = true;
          if (mq::CallTerminate(f)) {
            return;
          }
        }
        next = [&current, this]() {
          // LOCKED: Move the cursor forward.
          std::lock_guard<std::mutex> lock(mutex_);
          return Cursor::Next(current, list_);
        }();
        if (next.at_end) {
          // DIMA
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

}  // namespace bricks::persistence
}  // namespace bricks

#endif  // BRICKS_MQ_INTERFACE_PERSISTENCE_H
