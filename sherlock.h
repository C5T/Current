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
//   3) `void Terminate` or `bool Terminate()`:
//      This member function will be called if the listener has to be terminated externally,
//      which happens when the handler returned by `Subscribe()` goes out of scope.
//      If the signature is `bool Terminate()`, returning `false` will prevent the termination from happening,
//      allowing the user code to process more entries. Note that this can yield to the calling thread
//      waiting indefinitely.
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

  bool Entry(E& entry, size_t index, size_t total) {
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

  bool Terminate() {
    http_response_("{\"error\":\"The subscriber has terminated.\"}\n");
    return true;  // Confirm termination.
  }

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
  static bool DoIt(T&&) { return true; }
};

template <typename T>
struct CallTerminateImpl<T, true> {
  static decltype(std::declval<T>()->Terminate()) DoIt(T&& ptr) { return ptr->Terminate(); }
};

template <typename T, bool>
struct CallTerminateAndReturnBoolImpl {
  static bool DoIt(T&& ptr) {
    return CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::forward<T>(ptr));
  }
};

template <typename T>
struct CallTerminateAndReturnBoolImpl<T, true> {
  static bool DoIt(T&& ptr) {
    CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::forward<T>(ptr));
    return true;
  }
};

template <typename T>
bool CallTerminate(T&& ptr) {
  return CallTerminateAndReturnBoolImpl<
      T,
      std::is_same<void, decltype(CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::declval<T>()))>::
          value>::DoIt(ptr);
}

// TODO(dkorolev): Move this to Bricks. Cerealize uses it too, for `WithBaseType`.
template <typename T>
struct PretendingToBeUniquePtr {
  struct NullDeleter {
    void operator()(T&) {}
    void operator()(T*) {}
  };
  std::unique_ptr<T, NullDeleter> non_owned_instance_;
  PretendingToBeUniquePtr() = delete;
  PretendingToBeUniquePtr(T& instance) : non_owned_instance_(&instance) {}
  PretendingToBeUniquePtr(PretendingToBeUniquePtr&& rhs)
      : non_owned_instance_(std::move(rhs.non_owned_instance_)) {
    assert(non_owned_instance_);
    assert(!rhs.non_owned_instance_);
  }
  T* operator->() {
    assert(non_owned_instance_);
    return non_owned_instance_.get();
  }
};

template <typename T>
class StreamInstanceImpl {
 public:
  explicit StreamInstanceImpl(const std::string& name, const std::string& value_name)
      : name_(name), value_name_(value_name) {
    // TODO(dkorolev): Register this stream under this name.
    // TODO(dk+mz): Ensure the stream lives forever.
  }

  // `Publish()` and `Emplace()` return the index of the added entry.
  size_t Publish(const T& entry) {
    auto accesor = data_.MutableScopedAccessor();
    const size_t index = accesor->size();
    accesor->emplace_back(entry);
    return index;
  }

  size_t Publish(T&& entry) {
    auto accesor = data_.MutableScopedAccessor();
    const size_t index = accesor->size();
    accesor->emplace_back(std::move(entry));
    return index;
  }

  template <typename... ARGS>
  size_t Emplace(const ARGS&... entry_params) {
    // TODO(dkorolev): Am I not doing this C++11 thing right, or is it not yet supported?
    // data_.MutableUse([&entry_params](std::vector<T>& data) { data.emplace_back(entry_params...); });
    auto accesor = data_.MutableScopedAccessor();
    const size_t index = accesor->size();
    accesor->emplace_back(entry_params...);
    return index;
  }

  // `ListenerThread` spawns the thread and runs stream listener within it.
  //
  // Listener thread can always be `std::thread::join()`-ed. When this happens, the listener itself is notified
  // that the user has requested to join the listening thread, and then the thread is joined.
  // The listener code itself may choose to stop immediately, later, or never; in the latter cases `join()`-ing
  // the thread will run for a long time or forever.
  // Most commonly though, `join()` on a listener thread will result it in completing processing the entry
  // that it is currently processing, and then shutting down gracefully.
  // This makes working with C++ Sherlock almost as cozy as it could have been in node.js or Python.
  //
  // With respect to `std::thread::detach()`, we have two very different usecases:
  //
  // 1) If `ListenerThread` has been provided with a fair `unique_ptr<>` (which obviously is immediately
  //    `std::move()`-d into the thread, thus enabling `thread::detach()`), then `thread::detach()`
  //    is a legal operation, and it is the way to detach the listener from the caller thread,
  //    enabling to run the listener indefinitely, or until it itself decides to stop.
  //    The most notable example here would be spawning a listener to serve an HTTP request.
  //    (NOTE: This logic requires `Stream` objects to live forever. TODO(dk+mz): Make sure it is the case.)
  //
  // 2) The alternate usecase is when a stack-allocated object should acts as a listener.
  //    Implementation-wise, it is handled by wrapping the stack-allocated object into a `unique_ptr<>`
  //    with null deleter. Obviously, `thread::detach()` is then unsafe and thus impossible.
  //
  // The `ListenerScope` class handles the above two usecases, depending on whether a stack- or heap-allocated
  // instance of a listener has been passed into.
  template <typename F>
  class ListenerThread {
   private:
    struct CrossThreadsBlob {
      bricks::WaitableAtomic<std::vector<T>>& data;
      F listener;
      std::atomic_bool external_termination_request;
      std::atomic_bool thread_received_terminate_request;
      std::atomic_bool thread_done;

      CrossThreadsBlob(bricks::WaitableAtomic<std::vector<T>>& data, F&& listener)
          : data(data),
            listener(std::move(listener)),
            external_termination_request(false),
            thread_received_terminate_request(false),
            thread_done(false) {}

      CrossThreadsBlob() = delete;
      CrossThreadsBlob(const CrossThreadsBlob&) = delete;
      CrossThreadsBlob(CrossThreadsBlob&&) = delete;
      void operator=(const CrossThreadsBlob&) = delete;
      void operator=(CrossThreadsBlob&&) = delete;
    };

   public:
    ListenerThread(bricks::WaitableAtomic<std::vector<T>>& data, F&& listener)
        : data_(data),
          blob_(std::make_shared<CrossThreadsBlob>(data, std::move(listener))),
          thread_(&ListenerThread::StaticListenerThread, blob_) {}

    ~ListenerThread() {
      if (thread_.joinable()) {
        // TODO(dkorolev): Phrase the error message better.
        throw std::logic_error("Unrecoverable error in destructor: ListenerThread was not joined/detached.");
      }
    }

    void SafeJoinThread() {
      assert(thread_.joinable());  // TODO(dkorolev): Exception && test?
      // Buzz all active listeners. Ineffective, but does the right thing semantically, and passes the test.
      blob_->external_termination_request = true;
      data_.Notify();
      // This loop, along with polling logic in the thread, is our Chamberlain's Response to deadlocks.
      const auto now = bricks::time::Now();
      while (!blob_->thread_received_terminate_request && !blob_->thread_done) {
        // Spin lock if under 2ms. Start sending notifications nonstop afterwards.
        if (static_cast<uint64_t>(bricks::time::Now() - now) >= 2) {
          data_.Notify();
        }
      }
      // Wait for the thread to terminate.
      // Note that this code will only be executed if neither `Join()` nor `Detach()` has been done before.
      thread_.join();
    }

    void PotentiallyUnsafeDetachThread() {
      assert(thread_.joinable());  // TODO(dkorolev): Exception && test?
      thread_.detach();
    }

    static void StaticListenerThread(std::shared_ptr<CrossThreadsBlob> blob_shared_ptr) {
      CrossThreadsBlob* blob = blob_shared_ptr.get();
      assert(blob);
      size_t cursor = 0;
      volatile bool user_already_notified_to_terminate = false;
      volatile bool has_data;
      while (true) {
        has_data = false;
        blob->data.WaitFor(
            [&blob, &cursor, &user_already_notified_to_terminate, &has_data](const std::vector<T>& data) {
              if (!user_already_notified_to_terminate && blob->external_termination_request) {
                return true;
              } else if (data.size() > cursor) {
                has_data = true;
                return true;
              } else {
                return false;
              }
            },
            static_cast<bricks::time::MILLISECONDS_INTERVAL>(10));
        // This condition, along with the timeout in `WaitFor`, is our Chamberlain's Response to deadlocks.
        if (!user_already_notified_to_terminate && blob->external_termination_request) {
          blob->thread_received_terminate_request = true;
          user_already_notified_to_terminate = true;
          if (CallTerminate(blob->listener)) {
            break;
          }
        }
        if (has_data) {  // action == NEW_DATA_READY) {
          bool user_initiated_terminate = false;
          blob->data.ImmutableUse([&blob, &cursor, &user_initiated_terminate](const std::vector<T>& data) {
            // Entries are often instances of polymorphic types, that make it into various message queues.
            // The most straightforward way to store them is a `unique_ptr`, and the most straightforward
            // way to pass `unique_ptr`-s between threads is via `Emplace*(ptr.release())`.
            // If `Entry()` is being passed immutable records, the pain of cloning data from `unique_ptr`-s
            // becomes the user's pain. This shall not be allowed.
            //
            // Thus, here we need to make a copy.
            // The below implementation is imperfect, but it serves the purpose semantically.
            // TODO(dkorolev): Fix it.

            assert(cursor < data.size());
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
            if (!blob->listener->Entry(copy_of_entry, cursor, data.size())) {
              user_initiated_terminate = true;
            }
            ++cursor;
          });
          if (user_initiated_terminate) {
            break;
          }
        }
      }
      blob->thread_done = true;
    }

    bricks::WaitableAtomic<std::vector<T>>& data_;  // Just to `.Notify()` when terminating.
    std::shared_ptr<CrossThreadsBlob> blob_;
    std::thread thread_;

    ListenerThread(ListenerThread&&) = delete;  // Yep -- no move constructor.

    ListenerThread() = delete;
    ListenerThread(const ListenerThread&) = delete;
    void operator=(const ListenerThread&) = delete;
    void operator=(ListenerThread&&) = delete;
  };

  template <typename F>
  class AsyncListenerScope {
   public:
    AsyncListenerScope(bricks::WaitableAtomic<std::vector<T>>& data, F&& listener)
        : impl_(make_unique<ListenerThread<F>>(data, std::forward<F>(listener))) {}

    AsyncListenerScope(AsyncListenerScope&& rhs) : impl_(std::move(rhs.impl_)) {
      assert(impl_);
      assert(!rhs.impl_);
    }

    void Join() {
      assert(impl_);
      impl_->SafeJoinThread();
    }
    void Detach() {
      assert(impl_);
      impl_->PotentiallyUnsafeDetachThread();
    }

   private:
    std::unique_ptr<ListenerThread<F>> impl_;

    AsyncListenerScope() = delete;
    AsyncListenerScope(const AsyncListenerScope&) = delete;
    void operator=(const AsyncListenerScope&) = delete;
    void operator=(AsyncListenerScope&&) = delete;
  };

  template <typename F>
  class SyncListenerScope {
   public:
    SyncListenerScope(bricks::WaitableAtomic<std::vector<T>>& data, F&& listener)
        : joined_(false), impl_(make_unique<ListenerThread<F>>(data, std::move(listener))) {}

    SyncListenerScope(SyncListenerScope&& rhs) : joined_(false), impl_(std::move(rhs.impl_)) {
      // TODO(dkorolev): Constructor is not destructor -- we can make these exceptions and test them.
      assert(impl_);
      assert(!rhs.impl_);
      assert(!rhs.joined_);
      rhs.joined_ = true;  // Make sure no two joins are possible.
    }

    ~SyncListenerScope() {
      if (!joined_) {
        throw std::logic_error("Unrecoverable error in destructor: Join() was not called on for SyncListener.");
      }
    }

    void Join() {
      // TODO(dkorolev): Make these exceptions and test them.
      assert(!joined_);
      assert(impl_);
      impl_->SafeJoinThread();
      joined_ = true;
    }

   private:
    bool joined_;
    std::unique_ptr<ListenerThread<F>> impl_;

    SyncListenerScope() = delete;
    SyncListenerScope(const SyncListenerScope&) = delete;
    void operator=(const SyncListenerScope&) = delete;
    void operator=(SyncListenerScope&&) = delete;
  };

  // Expose the means to create both a sync ("scoped") and async ("detachable") listeners.
  template <typename F>
  AsyncListenerScope<F> AsyncSubscribeImpl(F&& listener) {
    // No `std::move()` needed: RAAI.
    return AsyncListenerScope<F>(data_, std::forward<F>(listener));
  }

  template <typename F>
  SyncListenerScope<PretendingToBeUniquePtr<F>> SyncSubscribeImpl(F& listener) {
    // No `std::move()` needed: RAAI.
    return SyncListenerScope<PretendingToBeUniquePtr<F>>(data_, PretendingToBeUniquePtr<F>(listener));
  }

  void ServeDataViaHTTP(Request r) {
    AsyncSubscribeImpl(make_unique<PubSubHTTPEndpoint<T>>(value_name_, std::move(r))).Detach();
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
  explicit StreamInstance(StreamInstanceImpl<T>* impl) : impl_(impl) {}

  size_t Publish(const T& entry) { return impl_->Publish(entry); }
  size_t Publish(T&& entry) { return impl_->Publish(std::move(entry)); }

  template <typename... ARGS>
  size_t Emplace(const ARGS&... entry_params) {
    return impl_->Emplace(entry_params...);
  }

  // TODO(dkorolev): EmplacePolymorphic, that does `new`.

  // TODO(dkorolev): Add a test for this code.
  // TODO(dkorolev): Perhaps eliminate the copy.
  template <typename E>
  typename std::enable_if<can_be_stored_in_unique_ptr<T, E>::value, size_t>::type Publish(const E& e) {
    // TODO(dkorolev): Don't rely on the existence of copy constructor.
    return impl_->Emplace(new E(e));
  }

  template <typename F>
  using SyncListenerScope =
      typename StreamInstanceImpl<T>::template SyncListenerScope<PretendingToBeUniquePtr<F>>;
  template <typename F>
  using AsyncListenerScope = typename StreamInstanceImpl<T>::template AsyncListenerScope<F>;

  // Synchonous subscription: `listener` is a stack-allocated object, and thus the listening thread
  // should ensure to terminate itself, when initiated from within the destructor of `SyncListenerScope`.
  // Note that the destructor of `SyncListenerScope` will wait until the listener terminates, thus,
  // not terminating as requested may result in the calling thread blocking for an unbounded amount of time.
  template <typename F>
  SyncListenerScope<bricks::rmconstref<F>> SyncSubscribe(F& listener) {
    // No `std::move()` needed: RAAI.
    return impl_->SyncSubscribeImpl(listener);
  }

  // Aynchonous subscription: `listener` is a heap-allocated object, the ownership of which
  // can be `std::move()`-d into the listening thread. It can be `Join()`-ed or `Detach()`-ed.
  template <typename F>
  AsyncListenerScope<bricks::rmconstref<F>> AsyncSubscribe(F&& listener) {
    // No `std::move()` needed: RAAI.
    return impl_->AsyncSubscribeImpl(std::forward<F>(listener));
  }

  void operator()(Request r) { impl_->ServeDataViaHTTP(std::move(r)); }
};

template <typename T>
StreamInstance<T> Stream(const std::string& name, const std::string& value_name = "entry") {
  // TODO(dkorolev): Validate stream name, add exceptions and tests for it.
  // TODO(dkorolev): Chat with the team if stream names should be case-sensitive, allowed symbols, etc.
  // TODO(dkorolev): Ensure no streams with the same name are being added. Add an exception for it.
  // TODO(dkorolev): Add the persistence layer.
  return StreamInstance<T>(new StreamInstanceImpl<T>(name, value_name));
}

}  // namespace sherlock

#endif  // SHERLOCK_H
