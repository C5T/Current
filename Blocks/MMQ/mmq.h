/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BLOCKS_MMQ_MMQ_H
#define BLOCKS_MMQ_MMQ_H

// MMQ is an efficient in-memory FIFO buffer.
// One of the objectives of MMQ is to minimize the time for which the thread publishing the message is blocked.
//
// Messages can be published into a MMQ via standard `Publish()` interface defined in `Blocks/SS/ss.h`.
// The consumer is run in a separate thread, and is fed one message at a time via `OnMessage()`.
//
// The buffer size, i.e. the number of the messages MMQ can hold, is defined by the constructor argument
// `buffer_size`. For usability reasons the default value for it can be set via `DEFAULT_BUFFER_SIZE`
// template argument.
//
// There are two possible strategies in case of buffer overflow (i.e. there is no free space to store message
// at the next call to `Publish()` or `Emplace()`):
//   1) Discard (drop) the message. In this case, the number of the messages dropped between the subseqent
//      calls of the consumer may be passed as a second argument of `OnMessage()`.
//   2) Block the publishing thread and wait until the next message is consumed and frees room in the buffer.
//      IMPORTANT NOTE: if there are several threads waiting to publish the message, MMQ DOES NOT guarantee that
//      the messages will be added in the order in which the functions were called. However, for any particular
//      thread, MMQ DOES GUARANTEE that the order of messages published from this thread will be respected.
//  Default behavior of MMQ is non-dropping and can be controlled via the `DROP_ON_OVERFLOW` template argument.

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../SS/ss.h"

#include "../../Bricks/time/chrono.h"

namespace current {
namespace mmq {

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
class MMQImpl {
  static_assert(current::ss::IsEntrySubscriber<CONSUMER, MESSAGE>::value, "");

 public:
  // Type of messages to store and dispatch.
  using message_t = MESSAGE;

  // This method will be called from one thread, which is spawned and owned by an instance of MMQImpl.
  // See "Blocks/SS/ss.h" and its test for possible callee signatures.
  using consumer_t = CONSUMER;

  MMQImpl(consumer_t& consumer, size_t buffer_size = DEFAULT_BUFFER_SIZE)
      : consumer_(consumer),
        circular_buffer_size_(buffer_size),
        circular_buffer_(circular_buffer_size_),
        consumer_thread_(&MMQImpl::ConsumerThread, this) {
    consumer_thread_created_ = true;
  }

  // Destructor waits for the consumer thread to terminate, which implies committing all the queued messages.
  ~MMQImpl() {
    if (consumer_thread_created_) {
      assert(consumer_thread_.joinable());
      {
        std::lock_guard<std::mutex> lock(mutex_);
        destructing_ = true;
        condition_variable_.notify_all();
      }
      consumer_thread_.join();
    }
  }

 protected:
  // Adds a message to the buffer.
  // Supports both copy and move semantics.
  // THREAD SAFE. Blocks the calling thread for as short period of time as possible.
  using MutexLockStatus = current::locks::MutexLockStatus;

  template <MutexLockStatus MLS>
  idxts_t DoPublish(const message_t& message, std::chrono::microseconds) {
    const std::pair<bool, size_t> index = CircularBufferAllocate();
    if (index.first) {
      circular_buffer_[index.second].message_body = message;
      CircularBufferCommit(index.second);
      return circular_buffer_[index.second].index_timestamp;
    } else {
      return idxts_t();
    }
  }

  template <MutexLockStatus MLS>
  idxts_t DoPublish(message_t&& message, std::chrono::microseconds) {
    const std::pair<bool, size_t> index = CircularBufferAllocate();
    if (index.first) {
      circular_buffer_[index.second].message_body = std::move(message);
      CircularBufferCommit(index.second);
      return circular_buffer_[index.second].index_timestamp;
    } else {
      return idxts_t();
    }
  }

  // template <typename... ARGS>
  // idxts_t DoEmplace(ARGS&&... args) {
  //   const std::pair<bool, size_t> index = CircularBufferAllocate();
  //   if (index.first) {
  //     circular_buffer_[index.second].message_body = message_t(std::forward<ARGS>(args)...);
  //     CircularBufferCommit(index.second);
  //     return circular_buffer_[index.second].index_timestamp;
  //   } else {
  //     return idxts_t();
  //   }
  // }

 private:
  MMQImpl(const MMQImpl&) = delete;
  MMQImpl(MMQImpl&&) = delete;
  void operator=(const MMQImpl&) = delete;
  void operator=(MMQImpl&&) = delete;

  // Increment the index respecting the circular nature of the buffer.
  void Increment(size_t& i) const { i = (i + 1) % circular_buffer_size_; }

  // The thread which extracts fully populated messages from the tail of the buffer
  // and feeds them to the consumer.
  void ConsumerThread() {
    // The `tail` pointer is local to the procesing thread.
    size_t tail = 0u;
    idxts_t save_last_idx_ts;

    while (true) {
      {
        // Get the next message, which is `READY` to be exported.
        // MUTEX-LOCKED, except for the condition variable part.
        std::unique_lock<std::mutex> lock(mutex_);
        condition_variable_.wait(
            lock, [this, tail] { return (circular_buffer_[tail].status == Entry::READY) || destructing_; });
        if (destructing_) {
          return;  // LCOV_EXCL_LINE
        }
        circular_buffer_[tail].status = Entry::BEING_EXPORTED;
        save_last_idx_ts = last_idx_ts_;
      }

      {
        // Then, export the message.
        // NO MUTEX REQUIRED.
        consumer_(std::move(circular_buffer_[tail].message_body),
                  circular_buffer_[tail].index_timestamp,
                  save_last_idx_ts);
      }

      {
        // Finally, mark the message entry in the buffer as `FREE` for overwriting.
        // MUTEX-LOCKED.
        {
          std::lock_guard<std::mutex> lock(mutex_);
          circular_buffer_[tail].status = Entry::FREE;
        }
        Increment(tail);

        // Need to notify message publishers that, in case they were waiting, a new slot is now available.
        // TODO(dkorolev) + TODO(mzhurovich): Think whether this might be a performance bottleneck.
        condition_variable_.notify_one();
      }
    }
  }

  // Returns { successful allocation flag, circular buffer index }.
  template <bool DROP = DROP_ON_OVERFLOW>
  typename std::enable_if<DROP, std::pair<bool, size_t>>::type CircularBufferAllocate() {
    // Implementation that discards the message if the queue is full.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    if (circular_buffer_[head_].status == Entry::FREE) {
      // Regular case.
      const size_t index = head_;
      ++last_idx_ts_.index;
      last_idx_ts_.us = current::time::Now();
      Increment(head_);
      circular_buffer_[index].status = Entry::BEING_IMPORTED;
      circular_buffer_[index].index_timestamp = last_idx_ts_;
      return std::make_pair(true, index);
    } else {
      // Overflow. Discarding the message.
      return std::make_pair(false, 0u);
    }
  }

  // Returns { successful allocation flag, circular buffer index }.
  template <bool DROP = DROP_ON_OVERFLOW>
  typename std::enable_if<!DROP, std::pair<bool, size_t>>::type CircularBufferAllocate() {
    // Implementation that waits for an empty space if the queue is full and blocks the calling thread
    // (potentially indefinitely, depends on the behavior of the consumer).
    // MUTEX-LOCKED.
    std::unique_lock<std::mutex> lock(mutex_);
    if (destructing_) {
      return std::make_pair(false, 0u);  // LCOV_EXCL_LINE
    }
    while (circular_buffer_[head_].status != Entry::FREE) {
      // Waiting for the next empty slot in the buffer.
      condition_variable_.wait(
          lock, [this] { return (circular_buffer_[head_].status == Entry::FREE) || destructing_; });
      if (destructing_) {
        return std::make_pair(false, 0u);  // LCOV_EXCL_LINE
      }
    }
    const size_t index = head_;
    ++last_idx_ts_.index;
    last_idx_ts_.us = current::time::Now();
    Increment(head_);
    circular_buffer_[index].status = Entry::BEING_IMPORTED;
    circular_buffer_[index].index_timestamp = last_idx_ts_;
    return std::make_pair(true, index);
  }

  void CircularBufferCommit(const size_t index) {
    // After the message has been copied over, mark it as `READY` for consumer.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    circular_buffer_[index].status = Entry::READY;
    condition_variable_.notify_all();
  }

  // The instance of the consuming side of the FIFO buffer.
  consumer_t& consumer_;

  // The capacity of the circular buffer for intermediate messages.
  // Messages beyond it will be dropped.
  const size_t circular_buffer_size_;

  // The `Entry` struct keeps the entries along with their completion status.
  struct Entry {
    idxts_t index_timestamp;
    message_t message_body;
    enum { FREE, BEING_IMPORTED, READY, BEING_EXPORTED } status = Entry::FREE;
  };

  // The circular buffer, of size `circular_buffer_size_`.
  // Entries are added/imported at `head_` and removed/exported at `tail`,
  // where `head_` is owned by the class instance and `tail` exists only in the consumer thread.
  std::vector<Entry> circular_buffer_;
  size_t head_ = 0u;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  idxts_t last_idx_ts_;

  // For safe thread destruction.
  bool destructing_ = false;

  // The thread in which the consuming process is running.
  std::thread consumer_thread_;
  bool consumer_thread_created_ = false;
};

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
using MMQ = ss::EntryPublisher<MMQImpl<MESSAGE, CONSUMER, DEFAULT_BUFFER_SIZE, DROP_ON_OVERFLOW>, MESSAGE>;

}  // namespace mmq
}  // namespace current

#endif  // BLOCKS_MMQ_MMQ_H
