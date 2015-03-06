#ifndef SHERLOCK_H
#define SHERLOCK_H

#include <vector>
#include <string>
#include <thread>

#include "waitable_atomic/waitable_atomic.h"

// Sherlock is the overlord of data storage and processing in KnowSheet.
// Its primary entity is the stream of data.
// Sherlock's streams are persistent, immutable, append-only typed sequences of records.
//
// The stream is defined by:
//
// 1) Type, which may be:
//    a) Single type for simple streams (ex. `UserRecord'),
//    b) Base class type for polymorphic streams (ex. `UserActionBase`), or
//    c) Base class plus a type list for real-time dispatching (ex. `LogEntry, tuple<Impression, Click>`).
//    TODO(dkorolev): The type should also present an unambiguous way to extract a timestamp of it.
//
// 2) Name, which is used for local storage and external access, most notably replication and subscriptions.
//
// 3) Optional type signature, to prevent data corruption when trying to serialize data using the wrong type.
//
// New streams are registred as `auto my_stream = sherlock::Stream<MyType>("my_stream");`.
//
// Sherlock runs as a singleton. The stream of a specific name can only added once.
// A user of C++ Sherlock interface should keep the return value of `my_stream`, as it provides the only
// access point to this stream's data.
//
// Sherlock streams can be published into and subscribed to.
// At any given point of time a stream can have zero or one publisher and any number of listeners.
//
// The publishing is done via `my_stream.Publish(MyType{...});`.
//
//   NOTE: It is the caller's full responsibility to ensure that:
//   1) Publishing is done from one thread only (since Sherlock offers no locking), and
//   2) The published entries come in non-decreasing order of their timestamps. TODO(dkorolev): Assert this.
//
// Subscription is done by via `my_stream.Subscribe(my_listener);`,
// where `my_listener` is an instance of the class doing the listening.
//
//   NOTE: that Sherlock runs each listener in a dedicated thread.
//
//   The `my_listener` object should expose the following member functions:
//
//   1) `bool Entry(const T_ENTRY& entry)`:
//      The `T_ENTRY` type is RTTI-dispatched against the supplied type list.
//      As long as `my_listener` returns `true`, it will keep receiving new entries,
//      which may end up blocking the thread until new, yet unseen, entries have been published.
//      Returning `false` will lead to no more entries passed to the listener, and the thread will be
//      terminated.
//
//   2) `void CaughtUp()`:
//      This method will be called as soon as the "historical data replay" phase is completed,
//      and the listener has entered the mode of serving the new entires coming in the real time.
//      This method is designed to flip some external variable or endpoint to the "healthy" state,
//      when the listener is considered eligible to serve incoming data requests.
//      TODO(dkorolev): Discuss the case if the listener falls behind again.
//
//   3) `bool TerminationRequest()`:
//      This member function will be called if the listener has to be terminated externally,
//      which happens when the handler returned by `Subscribe()` goes out of scope.
//      Much like with `Entry()`, if `TerminationRequest()` returns `false`,
//      no more methods of this listener object will be called and the listener thread will be terminated.
//      If it returns `true`, the calling thread of the `Subscribe()` call that started this listening process
//      will be blocked until this particular instance of the listener returns.
//      No further calls to `TerminationRequest()` will take place after the one and only call signaling
//      that the thread that initiated
//
// The call to `my_stream.Subscribe(my_listener);` launches the listener returns a handle object,
// the scope of which will define the lifetime of the listener.
//
// If the listener should run forever, use `my_stream.Subscribe().Join();`.
// It will block the calling thread unconditionally, until the listener itself decides to stop listening.

// An alternate solution is for the listener object to return `false` from `TerminationRequest()`.
// This will block the thread that created the listener at the exit of the scope that kept
// the return value of `Subscribe()`. This is a good solution for running local "jobs", i.e. "process 1000
// entries".
//
// TODO(dkorolev): Data persistence and tests.
// TODO(dkorolev): Add timestamps support and tests.
// TODO(dkorolev): Ensure the timestamps always come in a non-decreasing order.

namespace sherlock {

// FTR: This is really an inefficient reference implementatoin -- D.K.
template <typename T>
class StreamInstanceImpl {
 public:
  inline explicit StreamInstanceImpl(const std::string& name) {
    // TODO(dkorolev): Register this stream under this name.
    static_cast<void>(name);
  }

  inline void Publish(const T& entry) {
    data_.MutableUse([&entry](std::vector<T>& data) { data.emplace_back(entry); });
  }

  template <typename F>
  class ListenerScope {
   public:
    struct Internals {
      WaitableAtomic<std::vector<T>>& data;
      F&& listener;
      bool& flag;
      Internals(WaitableAtomic<std::vector<T>>& data, F&& listener, bool& flag)
          : data(data), listener(std::forward<F>(listener)), flag(flag) {}
      Internals() = delete;
      Internals(const Internals&) = delete;
      void operator=(const Internals&) = delete;
      Internals(Internals&&) = delete;
      void operator=(Internals&&) = delete;
    };
    inline ListenerScope(WaitableAtomic<std::vector<T>>& data, F&& listener)
        : data_(data),
          listener_thread_(&ListenerScope::ListenerThread,
                           new Internals(data, std::forward<F>(listener), external_terminate_flag_)) {}

    inline ~ListenerScope() {
      // Only do the extra termination work if the thread is not done and has not been joined or detached.
      // No extra action is required in either of three cases outlined above.
      if (listener_thread_.joinable()) {
        external_terminate_flag_ = true;
        data_.Notify();  // Buzz all active listeners. Ineffective, but does the right thing semantically.
        listener_thread_.join();
      }
    }

    inline void Join() {
      if (listener_thread_.joinable()) {
        listener_thread_.join();
      }
    }

    inline void Detach() {
      if (listener_thread_.joinable()) {
        listener_thread_.detach();
      }
    }

   private:
    inline static void ListenerThread(Internals* p_raw_internals) {
      std::unique_ptr<Internals> now_owned_internals(p_raw_internals);
      WaitableAtomic<std::vector<T>>& data = now_owned_internals->data;
      F&& listener = std::forward<F>(now_owned_internals->listener);
      bool& external_terminate_flag = now_owned_internals->flag;
      size_t cursor = 0;
      bool external_terminate_signal_sent = false;
      while (true) {
        bool internal_terminate_1 = false;
        data.Wait([&cursor, &internal_terminate_1, &external_terminate_flag, &external_terminate_signal_sent](
            const std::vector<T>& data) {
          if (external_terminate_flag) {
            if (!external_terminate_signal_sent) {
              internal_terminate_1 = true;
            }
          }
          return data.size() > cursor;
        });
        if (internal_terminate_1) {
          external_terminate_signal_sent = true;
          if (!listener.TerminationRequest()) {
            break;
          }
        }
        // TODO(dkorolev): RTTI dispatching here.
        bool internal_terminate_2 = false;
        data.ImmutableUse([&listener, &cursor, &internal_terminate_2](const std::vector<T>& data) {
          if (!listener.Entry(data[cursor])) {
            internal_terminate_2 = true;
          }
          ++cursor;
        });
        if (internal_terminate_2) {
          break;
        }
      }
    }

    WaitableAtomic<std::vector<T>>& data_;
    bool external_terminate_flag_ = false;
    std::thread listener_thread_;

    ListenerScope() = delete;
    ListenerScope(const ListenerScope&) = delete;
    void operator=(const ListenerScope&) = delete;
    ListenerScope(ListenerScope&&) = delete;
    void operator=(ListenerScope&&) = delete;
  };

  template <typename F>
  inline std::unique_ptr<ListenerScope<F>> Subscribe(F&& listener) {
    return std::unique_ptr<ListenerScope<F>>(new ListenerScope<F>(data_, std::forward<F>(listener)));
  }

 private:
  WaitableAtomic<std::vector<T>> data_;

  StreamInstanceImpl() = delete;
  StreamInstanceImpl(const StreamInstanceImpl&) = delete;
  void operator=(const StreamInstanceImpl&) = delete;
  StreamInstanceImpl(StreamInstanceImpl&&) = delete;
  void operator=(StreamInstanceImpl&&) = delete;
};

template <typename T>
struct StreamInstance {
  StreamInstanceImpl<T>* impl_;
  inline explicit StreamInstance(StreamInstanceImpl<T>* impl) : impl_(impl) {}
  inline void Publish(const T& entry) { impl_->Publish(entry); }
  template <typename F>
  inline std::unique_ptr<typename StreamInstanceImpl<T>::template ListenerScope<F>> Subscribe(F&& listener) {
    return impl_->Subscribe(std::forward<F>(listener));
  }
};

template <typename T>
inline StreamInstance<T> Stream(const std::string& name) {
  // TODO(dkorolev): Validate stream name, add exceptions and tests for it.
  // TODO(dkorolev): Chat with the team if stream names should be case-sensitive, allowed symbols, etc.
  // TODO(dkorolev): Ensure no streams with the same name are being added. Add an exception for it.
  // TODO(dkorolev): Add the persistence layer.
  return StreamInstance<T>(new StreamInstanceImpl<T>(name));
}

}  // namespace sherlock
#endif  // SHERLOCK_H
