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

#ifndef SHERLOCK_H
#define SHERLOCK_H

#define BRICKS_MOCK_TIME

#include "../Bricks/port.h"

#include <vector>
#include <string>
#include <mutex>
#include <memory>
#include <thread>
#include <type_traits>
#include <iostream>  // TODO(dkorolev): Remove it from here.

#include "../Bricks/net/api/api.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/template/rmref.h"
#include "../Bricks/waitable_atomic/waitable_atomic.h"

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
//   1) `bool Entry(const T_ENTRY& entry, size_t index, size_t total)`:
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

template <typename E>
struct ExtractTimestampImpl {
  static bricks::time::EPOCH_MILLISECONDS ExtractTimestamp(const E& entry) { return entry.ExtractTimestamp(); }
};

template <typename E>
struct ExtractTimestampImpl<std::unique_ptr<E>> {
  static bricks::time::EPOCH_MILLISECONDS ExtractTimestamp(const std::unique_ptr<E>& entry) {
    return entry->ExtractTimestamp();
  }
};

template <typename E>
bricks::time::EPOCH_MILLISECONDS ExtractTimestamp(E&& entry) {
  return ExtractTimestampImpl<bricks::rmref<E>>::ExtractTimestamp(std::forward<E>(entry));
}

template <typename E>
class PubSubHTTPEndpoint final {
 public:
  PubSubHTTPEndpoint(const std::string& value_name, Request r)
      : value_name_(value_name),
        http_request_(std::move(r)),
        http_response_(http_request_.SendChunkedResponse()) {
    if (http_request_.url.query.has("recent")) {
      serving_ = false;  // Start in 'non-serving' mode when `recent` is set.
      from_timestamp_ =
          r.timestamp - static_cast<bricks::time::MILLISECONDS_INTERVAL>(
                            bricks::strings::FromString<uint64_t>(http_request_.url.query["recent"]));
    }
    if (http_request_.url.query.has("n")) {
      serving_ = false;  // Start in 'non-serving' mode when `n` is set.
      bricks::strings::FromString(http_request_.url.query["n"], n_);
      cap_ = n_;  // If `?n=` parameter is set, it sets `cap_` too by default. Use `?n=...&cap=0` to override.
    }
    if (http_request_.url.query.has("n_min")) {
      // `n_min` is same as `n`, but it does not set the cap; just the lower bound for `recent`.
      serving_ = false;  // Start in 'non-serving' mode when `n_min` is set.
      bricks::strings::FromString(http_request_.url.query["n_min"], n_);
    }
    if (http_request_.url.query.has("cap")) {
      bricks::strings::FromString(http_request_.url.query["cap"], cap_);
    }
  }

  inline bool Entry(E& entry, size_t index, size_t total) {
    // TODO(dkorolev): Should we always extract the timestamp and throw an exception if there is a mismatch?
    try {
      if (!serving_) {
        const bricks::time::EPOCH_MILLISECONDS timestamp = ExtractTimestamp(entry);
        // Respect `n`.
        if (total - index <= n_) {
          serving_ = true;
        }
        // Respect `recent`.
        if (from_timestamp_ != static_cast<bricks::time::EPOCH_MILLISECONDS>(-1) &&
            timestamp >= from_timestamp_) {
          serving_ = true;
        }
      }
      if (serving_) {
        http_response_(entry, value_name_);
        if (cap_) {
          --cap_;
          if (!cap_) {
            return false;
          }
        }
      }
      return true;
    } catch (const bricks::net::NetworkException&) {
      return false;
    }
  }

  inline void Terminate() { http_response_("{\"error\":\"The subscriber has terminated.\"}\n"); }

 private:
  // Top-level JSON object name for Cereal.
  const std::string& value_name_;

  // `http_request_`:  need to keep the passed in request in scope for the lifetime of the chunked response.
  Request http_request_;

  // `http_response_`: the instance of the chunked response object to use.
  bricks::net::HTTPServerConnection::ChunkedResponseSender http_response_;

  // Conditions on which parts of the stream to serve.
  bool serving_ = true;
  // If set, the number of "last" entries to output.
  size_t n_ = 0;
  // If set, the hard limit on the maximum number of entries to output.
  size_t cap_ = 0;
  // If set, the timestamp from which the output should start.
  bricks::time::EPOCH_MILLISECONDS from_timestamp_ = static_cast<bricks::time::EPOCH_MILLISECONDS>(-1);

  PubSubHTTPEndpoint() = delete;
  PubSubHTTPEndpoint(const PubSubHTTPEndpoint&) = delete;
  void operator=(const PubSubHTTPEndpoint&) = delete;
  PubSubHTTPEndpoint(PubSubHTTPEndpoint&&) = delete;
  void operator=(PubSubHTTPEndpoint&&) = delete;
};

template <typename T>
constexpr bool HasTerminateMethod(char) {
  return false;
}

template <typename T>
constexpr auto HasTerminateMethod(int) -> decltype(std::declval<T>() -> Terminate(), bool()) {
  return true;
}

template <typename T, bool>
struct CallTerminateImpl {
  static void DoIt(T&&) {}
};

template <typename T>
struct CallTerminateImpl<T, true> {
  static void DoIt(T&& ptr) { ptr->Terminate(); }
};

template <typename T>
void CallTerminate(T&& ptr) {
  CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::forward<T>(ptr));
}

template <typename T>
class StreamInstanceImpl {
 public:
  inline explicit StreamInstanceImpl(const std::string& name, const std::string& value_name)
      : name_(name), value_name_(value_name) {
    // TODO(dkorolev): Register this stream under this name.
  }

  inline void Publish(const T& entry) {
    data_.MutableUse([&entry](std::vector<T>& data) { data.emplace_back(entry); });
  }

  template <typename... ARGS>
  inline void Emplace(const ARGS&... entry_params) {
    // TODO(dkorolev): Am I not doing this C++11 thing right, or is it not yet supported?
    // data_.MutableUse([&entry_params](std::vector<T>& data) { data.emplace_back(entry_params...); });
    auto scope = data_.MutableScopedAccessor();
    scope->emplace_back(entry_params...);
  }

  template <typename E>
  inline void PublishPolymorphic(const E& polymorphic_entry) {
    data_.MutableUse([&polymorphic_entry](std::vector<T>& data) { data.emplace_back(polymorphic_entry); });
  }

  template <typename F>
  class ListenerScope {
   public:
    inline explicit ListenerScope(bricks::WaitableAtomic<std::vector<T>>& data, OptionallyOwned<F> listener)
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
      inline Impl(bricks::WaitableAtomic<std::vector<T>>& data, OptionallyOwned<F> listener)
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
                                              bricks::WaitableAtomic<std::vector<T>>& data,
                                              OptionallyOwned<F> listener) {
        size_t cursor = 0;
        while (true) {
          data.Wait([&cursor, &terminate](const std::vector<T>& data) {
            return terminate->Get() || data.size() > cursor;
          });
          if (terminate->Get()) {
            CallTerminate(listener);
            break;
          }
          bool user_initiated_terminate = false;
          data.ImmutableUse([&listener, &cursor, &user_initiated_terminate](const std::vector<T>& data) {
            // Entries are often instances of polymorphic types, that make it into various message queues.
            // The most straightforward way to store them is a `unique_ptr`, and the most straightforward
            // way to pass `unique_ptr`-s between threads is via `Emplace*(ptr.release())`.
            // If `Entry()` is being passed immutable records, the pain of cloning data from `unique_ptr`-s
            // becomes the user's pain. This shall not be allowed.
            //
            // Thus, here we need to make a copy.
            // The below implementation is imperfect, but it serves the purpose semantically.
            // TODO(dkorolev): Fix it.

            const std::string json = JSON(data[cursor]);
            T copy_of_entry;
            try {
              ParseJSON(json, copy_of_entry);
            } catch (const std::exception& e) {
              std::cerr << "Something went terribly wrong." << std::endl;
              std::cerr << e.what();
              ::exit(-1);
            }

            // TODO(dkorolev): Perhaps RTTI dispatching here.
            if (!listener->Entry(copy_of_entry, cursor, data.size())) {
              user_initiated_terminate = true;
            }
            ++cursor;
          });
          if (user_initiated_terminate) {
            break;
          }
        }
      }

      const bool listener_is_detachable_;             // Indicates whether `Detach()` is legal.
      bricks::WaitableAtomic<std::vector<T>>& data_;  // Just to `.Notify()` when terminating.

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

  void ServeDataViaHTTP(Request r) {
    Subscribe(make_unique(new PubSubHTTPEndpoint<T>(value_name_, std::move(r)))).Detach();
  }

 private:
  const std::string name_;
  const std::string value_name_;
  // FTR: This is really an inefficient reference implementation. TODO(dkorolev): Revisit it.
  bricks::WaitableAtomic<std::vector<T>> data_;

  StreamInstanceImpl() = delete;
  StreamInstanceImpl(const StreamInstanceImpl&) = delete;
  void operator=(const StreamInstanceImpl&) = delete;
  StreamInstanceImpl(StreamInstanceImpl&&) = delete;
  void operator=(StreamInstanceImpl&&) = delete;
};

// TODO(dkorolev): Move into Bricks/util/ ?
template <typename B, typename E>
struct can_be_stored_in_unique_ptr {
  static constexpr bool value = false;
};

template <typename B, typename E>
struct can_be_stored_in_unique_ptr<std::unique_ptr<B>, E> {
  static constexpr bool value = std::is_same<B, E>::value || std::is_base_of<B, E>::value;
};

template <typename T>
struct StreamInstance {
  StreamInstanceImpl<T>* impl_;
  inline explicit StreamInstance(StreamInstanceImpl<T>* impl) : impl_(impl) {}

  inline void Publish(const T& entry) { impl_->Publish(entry); }

  template <typename... ARGS>
  inline void Emplace(const ARGS&... entry_params) {
    impl_->Emplace(entry_params...);
  }

  // TODO(dkorolev): Add a test for this code.
  // TODO(dkorolev): Perhaps eliminate the copy.
  template <typename E>
  typename std::enable_if<can_be_stored_in_unique_ptr<T, E>::value>::type Publish(const E& polymorphic_entry) {
    impl_->PublishPolymorphic(new E(polymorphic_entry));
  }

  template <typename F>
  inline typename StreamInstanceImpl<T>::template ListenerScope<F> Subscribe(F& listener) {
    return std::move(impl_->Subscribe(listener));
  }
  template <typename F>
  inline typename StreamInstanceImpl<T>::template ListenerScope<F> Subscribe(std::unique_ptr<F> listener) {
    return std::move(impl_->Subscribe(std::move(listener)));
  }

  void operator()(Request r) { impl_->ServeDataViaHTTP(std::move(r)); }

  template <typename F>
  using ListenerScope = typename StreamInstanceImpl<T>::template ListenerScope<F>;
};

template <typename T>
inline StreamInstance<T> Stream(const std::string& name, const std::string& value_name = "entry") {
  // TODO(dkorolev): Validate stream name, add exceptions and tests for it.
  // TODO(dkorolev): Chat with the team if stream names should be case-sensitive, allowed symbols, etc.
  // TODO(dkorolev): Ensure no streams with the same name are being added. Add an exception for it.
  // TODO(dkorolev): Add the persistence layer.
  return StreamInstance<T>(new StreamInstanceImpl<T>(name, value_name));
}

}  // namespace sherlock

#endif  // SHERLOCK_H
