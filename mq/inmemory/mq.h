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
  // It should expose one method, void OnMessage(T_MESSAGE&, size_t number_of_dropped_messages_if_any);
  // This method will be called from one thread, which is spawned and owned by an instance of MMQ.
  typedef CONSUMER T_CONSUMER;

  // The only constructor requires the refence to the instance of the consumer of entries.
  explicit MMQ(T_CONSUMER& consumer, size_t buffer_size = DEFAULT_BUFFER_SIZE)
      : consumer_(consumer),
        circular_buffer_size_(buffer_size),
        circular_buffer_(circular_buffer_size_),
        dropped_messages_count_(0u),
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
    if (index != static_cast<size_t>(-1)) {
      circular_buffer_[index].message_body = message;
      PushMessageCommit(index);
    }
  }

  void PushMessage(T_MESSAGE&& message) {
    const size_t index = PushMessageAllocate();
    if (index != static_cast<size_t>(-1)) {
      circular_buffer_[index].message_body = std::move(message);
      PushMessageCommit(index);
    }
  }

  template <typename... ARGS>
  void EmplaceMessage(ARGS&&... args) {
    const size_t index = PushMessageAllocate();
    if (index != static_cast<size_t>(-1)) {
      circular_buffer_[index].message_body = T_MESSAGE(args...);
      PushMessageCommit(index);
    }
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
    // The `tail` pointer is local to the procesing thread.
    size_t tail = 0u;

    // The counter of dropped messages is updated within the first, mutex-locked, section,
    // and then used in the second, mutex-free, one.
    size_t actually_dropped_messages;

    while (true) {
      {
        // First, get an message to export. Wait until it's finalized and ready to be exported.
        // MUTEX-LOCKED, except for the conditional variable part.
        std::unique_lock<std::mutex> lock(mutex_);
        while (circular_buffer_[tail].status != Entry::READY) {
          if (destructing_) {
            return;
          }
          condition_variable_.wait(
              lock, [this, tail] { return (circular_buffer_[tail].status == Entry::READY) || destructing_; });
        }
        if (destructing_) {
          return;
        }
        circular_buffer_[tail].status = Entry::BEING_EXPORTED;

        actually_dropped_messages = dropped_messages_count_;
        dropped_messages_count_ = 0u;
      }

      {
        // Then, export the message.
        // NO MUTEX REQUIRED.
        consumer_.OnMessage(std::ref(circular_buffer_[tail].message_body), actually_dropped_messages);
      }

      {
        // Finally, mark the message as successfully exported.
        // MUTEX-LOCKED.
        {
          std::lock_guard<std::mutex> lock(mutex_);
          circular_buffer_[tail].status = Entry::FREE;
        }
        Increment(tail);
      }
    }
  }

  size_t PushMessageAllocate() {
    // First, allocate room in the buffer for this message.
    // Overwrite the oldest message if have to.
    // MUTEX-LOCKED.
    std::unique_lock<std::mutex> lock(mutex_);
    if (circular_buffer_[head].status == Entry::FREE) {
      // Regular case.
      const size_t index = head;
      Increment(head);
      circular_buffer_[index].status = Entry::BEING_IMPORTED;
      return index;
    } else {
      // Buffer overflow, must drop the least recent element and keep the count of those.
      // TODO(mzhurovich): This logic sohuld go away for persisteny non-droping memory queue.
      for (size_t i = 0, candidate = head; i < circular_buffer_.size(); ++i, Increment(candidate)) {
        if (circular_buffer_[candidate].status == Entry::FREE) {
          circular_buffer_[candidate].status = Entry::BEING_IMPORTED;
          return candidate;
        }
      }
      // TODO(mzhurovich) + TODO(dkorolev): This should probably be a policy-driven thing,
      // along with the `if (index != static_cast<size_t>(-1))` conditions above.
      ++dropped_messages_count_;
      return static_cast<size_t>(-1);
    }
  }

  void PushMessageCommit(const size_t index) {
    // After the message has been copied over, mark it as finalized and advance `head_ready_`.
    // MUTEX-LOCKED.
    std::lock_guard<std::mutex> lock(mutex_);
    circular_buffer_[index].status = Entry::READY;
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
    enum { FREE, BEING_IMPORTED, READY, BEING_EXPORTED } status = Entry::FREE;
  };

  // The circular buffer, of size `circular_buffer_size_`.
  // Entries are added/imported at `head` and removed/exported at `tail`,
  // where `head` is owned by the class instance and `tail` exists only in the consumer thread.
  // While export is always sequential, depending on TODO policy, import may overwrite previously added entries.
  std::vector<Entry> circular_buffer_;
  size_t head = 0u;
  std::mutex mutex_;
  std::condition_variable condition_variable_;
  size_t dropped_messages_count_;

  // For safe thread destruction.
  bool destructing_ = false;

  // The thread in which the consuming process is running.
  std::thread consumer_thread_;
};

#endif  // BRICKS_MQ_INMEMORY_MQ_H
