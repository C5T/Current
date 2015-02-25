#ifndef BRICKS_MQ_INMEMORY_MQ_H
#define BRICKS_MQ_INMEMORY_MQ_H

// MMQ is an efficient in-memory FIFO buffer.
//
// Messages can be pushed into it via `PushMessage()`.
// The consumer is run in a separate thread, and is fed one message at a time via `OnMessage()`.
// The order of messages is preserved.
//
// One of the objectives of MMQ is to minimize the time for which the message pushing thread is blocked for.
//
// The default (and only so far) implementation discards older messages if the queue gets over capacity.
// TODO(dkorolev): This behavior should probably change.

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

template <typename CONSUMER, typename MESSAGE = std::string, size_t DEFAULT_BUFFER_SIZE = 1024>
class MMQ final {
 public:
  // Type of entries to store, defaults to `std::string`.
  typedef MESSAGE T_MESSAGE;

  // Type of the processor of the entries.
  // It should expose one method, void OnMessage(const T_MESSAGE&, size_t number_of_dropped_messages_if_any);
  // This method will be called from one thread, which is spawned and owned by an instance of MMQ.
  typedef CONSUMER T_CONSUMER;

  // The only constructor requires the refence to the instance of the consumer of entries.
  explicit MMQ(T_CONSUMER& consumer, size_t buffer_size = DEFAULT_BUFFER_SIZE)
      : consumer_(consumer),
        circular_buffer_size_(buffer_size),
        circular_buffer_(circular_buffer_size_),
        consumer_thread_(&MMQ::ConsumerThread, this) {}

  // Destructor waits for the consumer thread to terminate, which implies committing all the queued messages.
  ~MMQ() {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      destructing_ = true;
    }
    condition_variable_.notify_all();
    consumer_thread_.join();
  }

  // Adds a message to the buffer.
  // Supports both copy and move semantics.
  // THREAD SAFE. Blocks the calling thread for as short period of time as possible.
  void PushMessage(const T_MESSAGE& message) {
    const size_t index = PushMessageAllocate();
    circular_buffer_[index].message_body = message;
    PushMessageCommit(index);
  }

  void PushMessage(T_MESSAGE&& message) {
    const size_t index = PushMessageAllocate();
    circular_buffer_[index].message_body = std::move(message);
    PushMessageCommit(index);
  }

 private:
  MMQ(const MMQ&) = delete;
  MMQ(MMQ&&) = delete;
  void operator=(const MMQ&) = delete;
  void operator=(MMQ&&) = delete;

  // Increment the index respecting the circular nature of the buffer.
  void Increment(size_t& i) const { i = (i + 1) % circular_buffer_size_; }

  // The thread which extracts fully populated messages from the tail of the buffer and exports them.
  void ConsumerThread() {
    while (true) {
      size_t index;
      size_t this_time_dropped_messages;
      {
        // First, get an message to export. Wait until it's finalized and ready to be exported.
        // MUTEX-LOCKED, except for the conditional variable part.
        index = tail_;
        std::unique_lock<std::mutex> lock(mutex_);
        if (head_ready_ == tail_) {
          if (destructing_) {
            return;  // LCOV_EXCL_LINE
          }
          condition_variable_.wait(lock, [this] { return head_ready_ != tail_ || destructing_; });
          if (destructing_) {
            return;
          }
        }
        this_time_dropped_messages = number_of_dropped_messages_;
        number_of_dropped_messages_ = 0;
      }

      {
        // Then, export the message.
        // NO MUTEX REQUIRED.
        consumer_.OnMessage(circular_buffer_[index].message_body, this_time_dropped_messages);
      }

      {
        // Finally, mark the message as successfully exported.
        // MUTEX-LOCKED.
        std::lock_guard<std::mutex> lock(mutex_);
        Increment(tail_);
      }
    }
  }

  size_t PushMessageAllocate() {
    // First, allocate room in the buffer for this message.
    // Overwrite the oldest message if have to.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    const size_t index = head_allocated_;
    Increment(head_allocated_);
    if (head_allocated_ == tail_) {
      // Buffer overflow, must drop the least recent element and keep the count of those.
      ++number_of_dropped_messages_;
      if (tail_ == head_ready_) {
        Increment(head_ready_);  // LCOV_EXCL_LINE
      }
      Increment(tail_);
    }
    // Mark this message as incomplete, not yet ready to be sent over to the consumer.
    circular_buffer_[index].finalized = false;
    return index;
  }

  void PushMessageCommit(const size_t index) {
    // After the message has been copied over, mark it as finalized and advance `head_ready_`.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    circular_buffer_[index].finalized = true;
    while (head_ready_ != head_allocated_ && circular_buffer_[head_ready_].finalized) {
      Increment(head_ready_);
    }
    condition_variable_.notify_all();
  }

  // The instance of the consuming side of the FIFO buffer.
  T_CONSUMER& consumer_;

  // The capacity of the circular buffer for intermediate messages.
  // Messages beyond it will be dropped (the earlier messages will be overwritten).
  // TODO(dkorolev): Consider adding dropping policy. Often times block-waiting until the queue
  // can accept new messages, or simply returning `false` on a new message is a safer way.
  const size_t circular_buffer_size_;

  // The `Entry` struct keeps the entries along with the flag describing whether the message is done being
  // populated and thus is ready to be exported. The flag is neccesary, since the message at index `i+1` might
  // chronologically get finalized before the message at index `i` does.
  struct Entry {
    T_MESSAGE message_body;
    bool finalized;
  };

  // The circular buffer, of size `circular_buffer_size_`.
  std::vector<Entry> circular_buffer_;

  // The number of messages that have been overwritten due to buffer overflow.
  size_t number_of_dropped_messages_ = 0;

  // To minimize the time for which the message emitting thread is blocked for,
  // the buffer uses three "pointers":
  // 1) `tail_`: The index of the next element to be exported and removed from the buffer.
  // 2) `head_ready_`: The successor of the index of the element that is the last finalized element.
  // 3) `head_allocated_`: The index of the first unallocated element,
  //     into which the next message will be written.
  // The order of "pointers" is always tail_ <= head_ready_ <= head_allocated_.
  // The range [tail_, head_ready_) is what is ready to be extracted and sent over.
  // The range [head_ready_, head_allocated_) is the "grey area", where the entries are already
  // assigned indexes, but their population, done by respective client threads, is not done yet.
  // All three indexes are guarded by one mutex. (This can be improved, but meh. -- D.K.)
  size_t tail_ = 0;
  size_t head_ready_ = 0;
  size_t head_allocated_ = 0;
  std::mutex mutex_;
  std::condition_variable condition_variable_;

  // For safe thread destruction.
  bool destructing_ = false;

  // The thread in which the consuming process is running.
  std::thread consumer_thread_;
};

#endif  // BRICKS_MQ_INMEMORY_MQ_H
