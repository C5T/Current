/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BLOCKS_MMQ_MMPQ_H
#define BLOCKS_MMQ_MMPQ_H

// MMPQ is an in-memory priority queue, with the external interface loosely resembling the one of the original MMQ.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <set>

#include "../ss/ss.h"

#include "../../bricks/time/chrono.h"

namespace current {
namespace mmq {

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
class MMPQImpl {
  static_assert(current::ss::IsEntrySubscriber<CONSUMER, MESSAGE>::value, "");

 public:
  // The type of messages to store and dispatch.
  using message_t = MESSAGE;

  // Consumer's `operator()` will be called from a dedicated thread, which is spawned and owned
  // by the instance of MMPQImpl. See "blocks/ss/ss.h" and its test for possible callee signatures.
  using consumer_t = CONSUMER;

  MMPQImpl(consumer_t& consumer) : consumer_(consumer), consumer_thread_(&MMPQImpl::ConsumerThread, this) {
    consumer_thread_created_ = true;
  }

  // The destructor waits for the consumer thread to terminate, which implies committing all the queued messages.
  ~MMPQImpl() {
    if (consumer_thread_created_) {
      CURRENT_ASSERT(consumer_thread_.joinable());
      {
        std::lock_guard<std::mutex> lock(mutex_);
        destructing_ = true;
        condition_variable_.notify_all();
      }
      consumer_thread_.join();
    }
  }

  // Adds a message to the buffer. Supports both copy and move semantics. THREAD SAFE.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock,
            class E,
            typename TIMESTAMP>
  idxts_t PublisherPublishImpl(E&& entry, const TIMESTAMP timestamp) {
    locks::SmartMutexLockGuard<MLS> lock(mutex_);
    const auto us = current::time::TimestampAsMicroseconds(timestamp);
    ++last_idx_ts_.index;
    // Does not update `last_idx_ts_.us` at all. `UpdateHead()` must be called.
    // This is to ensure the regular `Publish`, coming through the interface defined in `Blocks/ss/pubsub.h`,
    // can publish into the future and utilize the full power of MMPQ.
    queue_.emplace(std::forward<E>(entry), idxts_t(last_idx_ts_.index, us));
    condition_variable_.notify_all();
    return last_idx_ts_;
  }

  // NOTE: `UpdateHead` must be called for the entries to be processed, since otherwise, as entries can be published
  // at non-increasing timestamps order, they must be held in the queue.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock, typename TIMESTAMP>
  void PublisherUpdateHeadImpl(const TIMESTAMP timestamp) {
    locks::SmartMutexLockGuard<MLS> lock(mutex_);
    const auto us = current::time::TimestampAsMicroseconds(timestamp);
    if (!(us > last_idx_ts_.us)) {
      CURRENT_THROW(ss::InconsistentTimestampException(last_idx_ts_.us + std::chrono::microseconds(1), us));
    }
    last_idx_ts_.us = us;
    condition_variable_.notify_all();
  }

 private:
  MMPQImpl(const MMPQImpl&) = delete;
  MMPQImpl(MMPQImpl&&) = delete;
  void operator=(const MMPQImpl&) = delete;
  void operator=(MMPQImpl&&) = delete;

  void ConsumerThread() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex_);

      condition_variable_.wait(lock, [this] {
        return (!queue_.empty() && queue_.begin()->index_timestamp.us <= last_idx_ts_.us) || destructing_;
      });

      if (destructing_) {
        return;  // LCOV_EXCL_LINE
      }

      auto it = queue_.begin();
      consumer_(std::move(const_cast<Entry&>(*it).message_body), it->index_timestamp, last_idx_ts_);
      queue_.erase(it);
    }
  }

  bool consumer_thread_created_ = false;

  // The instance of the consuming side of the FIFO buffer.
  consumer_t& consumer_;

  // The `Entry` struct keeps the entries along with their timestamps.
  struct Entry {
    idxts_t index_timestamp;
    message_t message_body;
    Entry() = default;
    Entry(Entry&&) = default;
    Entry(message_t&& message_body, idxts_t index_timestamp)
        : index_timestamp(index_timestamp), message_body(std::move(message_body)) {}
    bool operator<(const Entry& rhs) const { return index_timestamp.us < rhs.index_timestamp.us; }
  };

  std::set<Entry> queue_;
  idxts_t last_idx_ts_ = idxts_t(0, std::chrono::microseconds(-1));
  std::mutex mutex_;
  std::condition_variable condition_variable_;

  // For safe thread destruction.
  bool destructing_ = false;

  // The thread in which the consuming process is running.
  std::thread consumer_thread_;
};

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
using MMPQ = ss::EntryPublisher<MMPQImpl<MESSAGE, CONSUMER, DEFAULT_BUFFER_SIZE, DROP_ON_OVERFLOW>, MESSAGE>;

}  // namespace mmq
}  // namespace current

#endif  // BLOCKS_MMQ_MMPQ_H
