#ifndef SHERLOCK_H
#define SHERLOCK_H

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <thread>

#include "waitable_atomic/waitable_atomic.h"
#include "optionally_owned/optionally_owned.h"

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
// Sherlock runs as a singleton. The stream of a specific name can only be added once.
// TODO(dkorolev): Implement it this way. :-)
// A user of C++ Sherlock interface should keep the return value of `sherlock::Stream<T>`,
// as it is later used as the proxy to publish and subscribe to the data from this stream.
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
//      TODO(dkorolev): Implement it.
//      This method will be called as soon as the "historical data replay" phase is completed,
//      and the listener has entered the mode of serving the new entires coming in the real time.
//      This method is designed to flip some external variable or endpoint to the "healthy" state,
//      when the listener is considered eligible to serve incoming data requests.
//      TODO(dkorolev): Discuss the case if the listener falls behind again.
//
//   3) `void TerminationRequest()`:
//      This member function will be called if the listener has to be terminated externally,
//      which happens when the handler returned by `Subscribe()` goes out of scope.
//
// The call to `my_stream.Subscribe(my_listener);` launches the listener and returns
// an instance of a handle, the scope of which will define the lifetime of the listener.
//
// If the subscribing thread would like the listener to run forever, it can
// use use `.Join()` or `.Detach()` on the handle. `Join()` will block the calling thread unconditionally,
// until the listener itself decides to stop listening. `Detach()` will ensure
// that the ownership of the listener object has been transferred to the thread running the listener,
// and detach this thread to run in the background.
//
// TODO(dkorolev): Data persistence and tests.
// TODO(dkorolev): Add timestamps support and tests.
// TODO(dkorolev): Ensure the timestamps always come in a non-decreasing order.

namespace sherlock {

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
    inline explicit ListenerScope(WaitableAtomic<std::vector<T>>& data, OptionallyOwned<F> listener)
        : impl_(new Impl(data, std::move(listener))) {}

    // TODO(dkorolev): Think whether it's worth it to transfer the scope.
    // ListenerScope(ListenerScope&&) = delete;
    inline ListenerScope(ListenerScope&& rhs) : impl_(std::move(rhs.impl_)) {}

    inline void Join() { impl_->Join(); }
    inline void Detach() { impl_->Detach(); }

   private:
    struct ReallyAtomicFlag {
      // TODO(dkorolev): I couldn't figure out shared_ptr + atomic + memory orders at 3am.
      bool value_ = false;
      mutable std::mutex mutex_;
      bool Get() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return value_;
      }
      void Set() {
        std::unique_lock<std::mutex> lock(mutex_);
        value_ = true;
      }
    };

    class Impl {
     public:
      inline Impl(WaitableAtomic<std::vector<T>>& data, OptionallyOwned<F> listener)
          : listener_is_detachable_(listener.IsDetachable()),
            data_(data),
            terminate_flag_(new ReallyAtomicFlag()),
            listener_thread_(
                &Impl::StaticListenerThread, terminate_flag_, std::ref(data), std::move(listener)) {}

      inline ~Impl() {
        // Indicate to the listener thread that the listener handle is going out of scope.
        if (listener_thread_.joinable()) {
          // Buzz all active listeners. Ineffective, but does the right thing semantically, and passes the test.
          terminate_flag_->Set();
          data_.Notify();
          // Wait for the thread to terminate.
          // Note that this code will only be executed if neither `Join()` nor `Detach()` has been done before.
          listener_thread_.join();
        }
      }

      inline void Join() {
        if (listener_thread_.joinable()) {
          listener_thread_.join();
        }
      }

      inline void Detach() {
        if (listener_is_detachable_) {
          if (listener_thread_.joinable()) {
            listener_thread_.detach();
          }
        } else {
          // TODO(dkorolev): Custom exception type here.
          throw std::logic_error("Can not detach a non-unique_ptr-created listener.");
        }
      }

      inline static void StaticListenerThread(std::shared_ptr<ReallyAtomicFlag> terminate,
                                              WaitableAtomic<std::vector<T>>& data,
                                              OptionallyOwned<F> listener) {
        size_t cursor = 0;
        while (true) {
          data.Wait([&cursor, &terminate](const std::vector<T>& data) {
            return terminate->Get() || data.size() > cursor;
          });
          if (terminate->Get()) {
            listener->Terminate();
            break;
          }
          bool user_initiated_terminate = false;
          data.ImmutableUse([&listener, &cursor, &user_initiated_terminate](const std::vector<T>& data) {
            // TODO(dkorolev): RTTI dispatching here.
            if (!listener->Entry(data[cursor])) {
              user_initiated_terminate = true;
            }
            ++cursor;
          });
          if (user_initiated_terminate) {
            break;
          }
        }
      }

      const bool listener_is_detachable_;     // Indicates whether `Detach()` is legal.
      WaitableAtomic<std::vector<T>>& data_;  // Just to `.Notify()` when terminating.

      // A termination flag that outlives both the `Impl` and its thread.
      std::shared_ptr<ReallyAtomicFlag> terminate_flag_;

      // The thread that runs the listener. It owns the actual `OptionallyOwned<F>` handler;
      std::thread listener_thread_;

      Impl() = delete;
      Impl(const Impl&) = delete;
      Impl(Impl&&) = delete;
      void operator=(const Impl&) = delete;
      void operator=(Impl&&) = delete;
    };

    std::unique_ptr<Impl> impl_;

    ListenerScope() = delete;
    ListenerScope(const ListenerScope&) = delete;
    void operator=(const ListenerScope&) = delete;
    void operator=(ListenerScope&&) = delete;
  };

  // There are two forms of `Subscribe()`: One takes a reference to the listener, and one takes a `unique_ptr`.
  // They both go through `OptionallyOwned<F>`, with the one taking a `unique_ptr` being detachable,
  // while the one taking a reference to the listener being scoped-only.
  // I'd rather implement them as two separate methods to avoid any confusions. - D.K.
  template <typename F>
  inline ListenerScope<F> Subscribe(F& listener) {
    return std::move(ListenerScope<F>(data_, OptionallyOwned<F>(listener)));
  }
  template <typename F>
  inline ListenerScope<F> Subscribe(std::unique_ptr<F>&& listener) {
    return std::move(ListenerScope<F>(data_, OptionallyOwned<F>(std::move(listener))));
  }

 private:
  // FTR: This is really an inefficient reference implementation. TODO(dkorolev): Revisit it.
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
  inline typename StreamInstanceImpl<T>::template ListenerScope<F> Subscribe(F& listener) {
    return std::move(impl_->Subscribe(listener));
  }
  template <typename F>
  inline typename StreamInstanceImpl<T>::template ListenerScope<F> Subscribe(std::unique_ptr<F> listener) {
    return std::move(impl_->Subscribe(std::move(listener)));
  }
  template <typename F>
  inline typename StreamInstanceImpl<T>::template ListenerScope<F> Subscribe(F* listener) {
    return std::move(Subscribe(std::unique_ptr<F>(listener)));
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
