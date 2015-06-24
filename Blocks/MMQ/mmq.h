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

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../SS/ss.h"

namespace blocks {

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
class MMQImpl {
 public:
  // Type of messages to store and dispatch.
  typedef MESSAGE T_MESSAGE;

  // This method will be called from one thread, which is spawned and owned by an instance of MMQImpl.
  // See "Blocks/SS/ss.h" and its test for possible callee signatures.
  typedef CONSUMER T_CONSUMER;

  MMQImpl(std::function<MESSAGE(const MESSAGE&)> clone,
          T_CONSUMER& consumer,
          size_t buffer_size = DEFAULT_BUFFER_SIZE)
      : clone_(clone),
        consumer_(consumer),
        circular_buffer_size_(buffer_size),
        circular_buffer_(circular_buffer_size_),
        consumer_thread_(&MMQImpl::ConsumerThread, this) {}

  // Destructor waits for the consumer thread to terminate, which implies committing all the queued messages.
  ~MMQImpl() {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    consumer_thread_.join();
  }

 protected:
  // Adds a message to the buffer.
  // Supports both copy and move semantics.
  // THREAD SAFE. Blocks the calling thread for as short period of time as possible.
  size_t DoPublish(const T_MESSAGE& message) {
    const std::pair<size_t, size_t> index = CircularBufferAllocate();
    if (index.second) {
      circular_buffer_[index.first].absolute_index = index.second - 1u;
      circular_buffer_[index.first].message_body = std::move(clone_(message));
      CircularBufferCommit(index.first);
      return index.second;
    } else {
      return 0u;
    }
  }

  size_t DoPublish(T_MESSAGE&& message) {
    const std::pair<size_t, size_t> index = CircularBufferAllocate();
    if (index.second) {
      circular_buffer_[index.first].absolute_index = index.second - 1u;
      circular_buffer_[index.first].message_body = std::move(message);
      CircularBufferCommit(index.first);
      return index.second;
    } else {
      return 0u;
    }
  }

  template <typename... ARGS>
  size_t DoEmplace(ARGS&&... args) {
    const std::pair<size_t, size_t> index = CircularBufferAllocate();
    if (index.second) {
      circular_buffer_[index.first].absolute_index = index.second - 1u;
      circular_buffer_[index.first].message_body = T_MESSAGE(std::forward<ARGS>(args)...);
      CircularBufferCommit(index.first);
      return index.second;
    } else {
      return 0u;
    }
  }

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
    size_t save_total_messages;

    while (true) {
      {
        // Get the next message, which is `READY` to be exported.
        // MUTEX-LOCKED, except for the condition variable part.
        std::unique_lock<std::mutex> lock(mutex_);
        while (circular_buffer_[tail].status != Entry::READY) {
          if (destructing_) {
            return;
          }
          condition_variable_.wait(
              lock, [this, tail] { return (circular_buffer_[tail].status == Entry::READY) || destructing_; });
        }
        if (destructing_) {
          return;  // LCOV_EXCL_LINE
        }
        circular_buffer_[tail].status = Entry::BEING_EXPORTED;
        save_total_messages = total_messages_;
      }

      {
        // Then, export the message.
        // NO MUTEX REQUIRED.
        blocks::ss::DispatchEntryByRValue(consumer_,
                                          std::move(circular_buffer_[tail].message_body),
                                          circular_buffer_[tail].absolute_index,
                                          save_total_messages);
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

  // Returns { circular buffer index, absolute 1-based message index, or zero if the message is dropped }.
  template <bool DROP = DROP_ON_OVERFLOW>
  typename std::enable_if<DROP, std::pair<size_t, size_t>>::type CircularBufferAllocate() {
    // Implementation that discards the message if the queue is full.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    if (circular_buffer_[head_].status == Entry::FREE) {
      // Regular case.
      ++total_messages_;
      const size_t index = head_;
      Increment(head_);
      circular_buffer_[index].status = Entry::BEING_IMPORTED;
      return std::make_pair(index, total_messages_);
    } else {
      // Overflow. Discarding the message. The second `0u` indicates the message should be dropped.
      return std::make_pair(0u, 0u);
    }
  }

  // Returns { circular buffer index, absolute message index }.
  template <bool DROP = DROP_ON_OVERFLOW>
  typename std::enable_if<!DROP, std::pair<size_t, size_t>>::type CircularBufferAllocate() {
    // Implementation that waits for an empty space if the queue is full and blocks the calling thread
    // (potentially indefinitely, depends on the behavior of the consumer).
    // MUTEX-LOCKED.
    std::unique_lock<std::mutex> lock(mutex_);
    if (destructing_) {
      return std::make_pair(0u, 0u);  // LCOV_EXCL_LINE
    }
    while (circular_buffer_[head_].status != Entry::FREE) {
      // Waiting for the next empty slot in the buffer.
      condition_variable_.wait(
          lock, [this] { return (circular_buffer_[head_].status == Entry::FREE) || destructing_; });
      if (destructing_) {
        return std::make_pair(0u, 0u);  // LCOV_EXCL_LINE
      }
    }
    const size_t index = head_;
    ++total_messages_;
    Increment(head_);
    circular_buffer_[index].status = Entry::BEING_IMPORTED;
    return std::make_pair(index, total_messages_);
  }

  void CircularBufferCommit(const size_t index) {
    // After the message has been copied over, mark it as `READY` for consumer.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    circular_buffer_[index].status = Entry::READY;
    condition_variable_.notify_all();
  }

  // The method used to clone incoming entries.
  std::function<MESSAGE(const MESSAGE&)> clone_;

  // The instance of the consuming side of the FIFO buffer.
  T_CONSUMER& consumer_;

  // The capacity of the circular buffer for intermediate messages.
  // Messages beyond it will be dropped.
  const size_t circular_buffer_size_;

  // The `Entry` struct keeps the entries along with their completion status.
  struct Entry {
    size_t absolute_index;
    T_MESSAGE message_body;
    enum { FREE, BEING_IMPORTED, READY, BEING_EXPORTED } status = Entry::FREE;
  };

  // The circular buffer, of size `circular_buffer_size_`.
  // Entries are added/imported at `head_` and removed/exported at `tail`,
  // where `head_` is owned by the class instance and `tail` exists only in the consumer thread.
  std::vector<Entry> circular_buffer_;
  size_t head_ = 0u;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  size_t total_messages_ = 0u;

  // For safe thread destruction.
  bool destructing_ = false;

  // The thread in which the consuming process is running.
  std::thread consumer_thread_;
};

template <typename MESSAGE, typename CONSUMER, size_t DEFAULT_BUFFER_SIZE = 1024, bool DROP_ON_OVERFLOW = false>
using MMQ = ss::Publisher<MMQImpl<MESSAGE, CONSUMER, DEFAULT_BUFFER_SIZE, DROP_ON_OVERFLOW>, MESSAGE>;

}  // namespace blocks

#endif  // BLOCKS_MMQ_MMQ_H
