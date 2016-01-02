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

#include "../port.h"

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "pubsub.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/Persistence/persistence.h"
#include "../Blocks/SS/ss.h"

#include "../Bricks/template/decay.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/util/clone.h"
#include "../Bricks/util/null_deleter.h"
#include "../Bricks/util/waitable_terminate_signal.h"

// Sherlock is the overlord of data storage and processing in Current.
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
// A user of C++ Sherlock interface should keep the return value of `sherlock::Stream<ENTRY>`,
// as it is later used as the proxy to publish and subscribe to the data from this stream.
//
// Sherlock streams can be published into and subscribed to.
// At any given point of time a stream can have zero or one publisher and any number of listeners.
//
// The publishing is done via `my_stream.Publish(MyType{...});`.
//
//   NOTE: It is the caller's full responsibility to ensure that:
//   1) Publishing is done from one thread only (since Sherlock offers no locking), and
//   2) The published entries come in strictly increasing order of their timestamps. TODO(dkorolev): Assert this.
//
// Subscription is done by via `my_stream.Subscribe(my_listener);`,
// where `my_listener` is an instance of the class doing the listening.
//
//   NOTE: that Sherlock runs each listener in a dedicated thread.
//
//   The `my_listener` object should expose the following member functions:
//
//   1) `{void/bool} operator()({const ENTRY& / ENTRY&&} entry [, size_t index [, size_t total]]):
//
//      The `ENTRY` type is RTTI-dispatched against the supplied type list.
//      As long as `my_listener` returns `true`, it will keep receiving new entries,
//      which may end up blocking the thread until new, yet unseen, entries have been published.
//
//      If `operator()` is defined as `bool`, returning `false` will tell Sherlock that no more entries
//      should be passed to this listener, and the thread serving this listener should be terminated.
//
//      More details on the calling convention: "Bricks/mq/interface/interface.h"
//
//   2) `void CaughtUp()`:
//
//      TODO(dkorolev): Implement it.
//      This method will be called as soon as the "historical data replay" phase is completed,
//      and the listener has entered the mode of serving the new entires coming in the real time.
//      This method is designed to flip some external variable or endpoint to the "healthy" state,
//      when the listener is considered eligible to serve incoming data requests.
//      TODO(dkorolev): Discuss the case if the listener falls behind again.
//
//   3) `void Terminate` or `bool Terminate()`:
//
//      This member function will be called if the listener has to be terminated externally,
//      which happens when the handler returned by `Subscribe()` goes out of scope.
//      If the signature is `bool Terminate()`, returning `false` will prevent the termination from happening,
//      allowing the user code to process more entries. Note that this can yield to the calling thread
//      waiting indefinitely.
//
// The call to `my_stream.Subscribe(my_listener);` launches the listener and returns
// an instance of a handle, the scope of which will define the lifetime of the listener.
//
// If the subscribing thread would like the listener to run forever, it can use `.Join()` or `.Detach()`
// on the handle. `Join()` will block the calling thread unconditionally, until the listener itself
// decides to stop listening. `Detach()` will ensure that the ownership of the listener object
// has been transferred to the thread running the listener, and detach this thread to run in the background.
//
// TODO(dkorolev): Add timestamps support and tests.
// TODO(dkorolev): Ensure the timestamps always come in a non-decreasing order.

namespace sherlock {

template <typename ENTRY, template <typename, typename> class PERSISTENCE_LAYER, class CLONER>
class StreamInstanceImpl {
 public:
  typedef ENTRY T_ENTRY;
  typedef PERSISTENCE_LAYER<ENTRY, CLONER> T_PERSISTENCE_LAYER;

  // TODO(dkorolev): In constuctor: Register the stream under its name in a singleton.
  // TODO(dkorolev): In constuctor: Ensure the stream lives forever.

  StreamInstanceImpl(const std::string& name)
      : name_(name), storage_(std::make_shared<T_PERSISTENCE_LAYER>()) {}

  template <typename EXTRA_PARAM>
  StreamInstanceImpl(const std::string& name, EXTRA_PARAM&& extra_param)
      : name_(name), storage_(std::make_shared<T_PERSISTENCE_LAYER>(std::forward<EXTRA_PARAM>(extra_param))) {}

  // `Publish()` and `Emplace()` return the index of the added entry.
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  size_t Publish(const ENTRY& entry) { return storage_->Publish(entry); }
  size_t Publish(ENTRY&& entry) { return storage_->Publish(std::move(entry)); }

  // When `ENTRY` is an `std::unique_ptr<>`, support two more `Publish()` syntaxes for entries of derived types.
  // 1) `Publish(const DERIVED_ENTRY&)`, and
  // 2) `Publish(conststd::unique_ptr<DERIVED_ENTRY>&)`.
  template <typename DERIVED_ENTRY>
  typename std::enable_if<current::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, size_t>::type
  Publish(const DERIVED_ENTRY& e) {
    return storage_->Publish(e);
  }

  template <typename DERIVED_ENTRY>
  typename std::enable_if<current::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, size_t>::type
  Publish(const std::unique_ptr<DERIVED_ENTRY>& e) {
    return storage_->Publish(e);
  }

  template <typename... ARGS>
  size_t Emplace(ARGS&&... entry_params) {
    return storage_->Emplace(std::forward<ARGS>(entry_params)...);
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
    // This guy is a `shared_ptr<>` itself later on, to ensure its lifetime.
    struct ListenerThreadSharedState {
      F listener;  // The ownership of the listener is transferred to instance of this class.
      std::shared_ptr<T_PERSISTENCE_LAYER> storage;

      current::WaitableTerminateSignal terminate_signal;

      ListenerThreadSharedState(std::shared_ptr<T_PERSISTENCE_LAYER> storage, F&& listener)
          : listener(std::move(listener)), storage(storage) {}

      ListenerThreadSharedState() = delete;
      ListenerThreadSharedState(const ListenerThreadSharedState&) = delete;
      ListenerThreadSharedState(ListenerThreadSharedState&&) = delete;
      void operator=(const ListenerThreadSharedState&) = delete;
      void operator=(ListenerThreadSharedState&&) = delete;
    };

   public:
    ListenerThread(std::shared_ptr<T_PERSISTENCE_LAYER> storage, F&& listener)
        : state_(std::make_shared<ListenerThreadSharedState>(storage, std::forward<F>(listener))),
          thread_(&ListenerThread::StaticListenerThread, state_) {}

    ~ListenerThread() {
      if (thread_.joinable()) {
        // TODO(dkorolev): Phrase the error message better.
        // LCOV_EXCL_START
        std::cerr << "Unrecoverable error in destructor: ListenerThread was not joined/detached.\n";
        std::exit(-1);
        // LCOV_EXCL_STOP
      }
    }

    void SafeJoinThread() {
      assert(thread_.joinable());  // TODO(dkorolev): Exception && test?

      state_->terminate_signal.SignalExternalTermination();

      // Wait for the thread to terminate.
      // Note that this code will only be executed if neither `Join()` nor `Detach()` has been done before.
      thread_.join();
    }

    void PotentiallyUnsafeDetachThread() {
      assert(thread_.joinable());  // TODO(dkorolev): Exception && test?
      thread_.detach();
    }

    static void StaticListenerThread(std::shared_ptr<ListenerThreadSharedState> state) {
      state->storage->SyncScanAllEntries(state->terminate_signal, *state->listener);
    }

    std::shared_ptr<ListenerThreadSharedState> state_;
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
    AsyncListenerScope(std::shared_ptr<T_PERSISTENCE_LAYER> storage, F&& listener)
        : impl_(make_unique<ListenerThread<F>>(storage, std::forward<F>(listener))) {}

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
    SyncListenerScope(std::shared_ptr<T_PERSISTENCE_LAYER> storage, F&& listener)
        : joined_(false), impl_(make_unique<ListenerThread<F>>(storage, std::move(listener))) {}

    SyncListenerScope(SyncListenerScope&& rhs) : joined_(false), impl_(std::move(rhs.impl_)) {
      // TODO(dkorolev): Constructor is not destructor -- we can make these exceptions and test them.
      assert(impl_);
      assert(!rhs.impl_);
      assert(!rhs.joined_);
      rhs.joined_ = true;  // Make sure no two joins are possible.
    }

    ~SyncListenerScope() {
      if (!joined_) {
        // LCOV_EXCL_START
        std::cerr << "Unrecoverable error in destructor: Join() was not called on for SyncListener.\n";
        std::exit(-1);
        // LCOV_EXCL_STOP
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
    return AsyncListenerScope<F>(storage_, std::forward<F>(listener));
  }

  template <typename F>
  SyncListenerScope<std::unique_ptr<F, current::NullDeleter>> SyncSubscribeImpl(F& listener) {
    // RAAI, no `std::move()` needed.
    return SyncListenerScope<std::unique_ptr<F, current::NullDeleter>>(
        storage_, std::unique_ptr<F, current::NullDeleter>(&listener));
  }

  template <JSONFormat J = JSONFormat::Current>
  void ServeDataViaHTTP(Request r) {
    AsyncSubscribeImpl(make_unique<PubSubHTTPEndpoint<ENTRY, J>>(std::move(r))).Detach();
  }

 private:
  const std::string name_;
  std::shared_ptr<T_PERSISTENCE_LAYER> storage_;

  StreamInstanceImpl() = delete;
  StreamInstanceImpl(const StreamInstanceImpl&) = delete;
  void operator=(const StreamInstanceImpl&) = delete;
  StreamInstanceImpl(StreamInstanceImpl&&) = delete;
  void operator=(StreamInstanceImpl&&) = delete;
};

template <typename ENTRY, template <typename, typename> class PERSISTENCE_LAYER, class CLONER>
struct StreamInstance {
  typedef ENTRY T_ENTRY;
  typedef PERSISTENCE_LAYER<ENTRY, CLONER> T_PERSISTENCE_LAYER;

  StreamInstanceImpl<ENTRY, PERSISTENCE_LAYER, CLONER>* impl_;

  explicit StreamInstance(StreamInstanceImpl<ENTRY, PERSISTENCE_LAYER, CLONER>* impl) : impl_(impl) {}

  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  size_t Publish(const ENTRY& entry) { return impl_->Publish(entry); }
  size_t Publish(ENTRY&& entry) { return impl_->Publish(std::move(entry)); }

  // Support two syntaxes of `Publish` as well.
  template <typename DERIVED_ENTRY>
  typename std::enable_if<current::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, size_t>::type
  Publish(const DERIVED_ENTRY& e) {
    return impl_->Publish(e);
  }

  template <typename DERIVED_ENTRY>
  typename std::enable_if<current::can_be_stored_in_unique_ptr<ENTRY, DERIVED_ENTRY>::value, size_t>::type
  Publish(const std::unique_ptr<DERIVED_ENTRY>& e) {
    return impl_->Publish(e);
  }

  template <typename... ARGS>
  size_t Emplace(ARGS&&... entry_params) {
    return impl_->Emplace(std::forward<ARGS>(entry_params)...);
  }

  template <typename F>
  using SyncListenerScope =
      typename StreamInstanceImpl<ENTRY, PERSISTENCE_LAYER, CLONER>::template SyncListenerScope<
          std::unique_ptr<F, current::NullDeleter>>;
  template <typename F>
  using AsyncListenerScope =
      typename StreamInstanceImpl<ENTRY, PERSISTENCE_LAYER, CLONER>::template AsyncListenerScope<F>;

  // Synchonous subscription: `listener` is a stack-allocated object, and thus the listening thread
  // should ensure to terminate itself, when initiated from within the destructor of `SyncListenerScope`.
  // Note that the destructor of `SyncListenerScope` will wait until the listener terminates, thus,
  // not terminating as requested may result in the calling thread blocking for an unbounded amount of time.
  template <typename F>
  SyncListenerScope<current::decay<F>> SyncSubscribe(F& listener) {
    // No `std::move()` needed: RAAI.
    return impl_->SyncSubscribeImpl(listener);
  }

  // Aynchonous subscription: `listener` is a heap-allocated object, the ownership of which
  // can be `std::move()`-d into the listening thread. It can be `Join()`-ed or `Detach()`-ed.
  template <typename F>
  AsyncListenerScope<current::decay<F>> AsyncSubscribe(F&& listener) {
    // No `std::move()` needed: RAAI.
    return impl_->AsyncSubscribeImpl(std::forward<F>(listener));
  }

  template <JSONFormat J = JSONFormat::Current>
  void ServeDataViaHTTP(Request r) {
    impl_->template ServeDataViaHTTP<J>(std::move(r));
  }

  void operator()(Request r) { ServeDataViaHTTP(std::move(r)); }
};

// TODO(dkorolev): Validate stream name, add exceptions and tests for it.
// TODO(dkorolev): Chat with the team if stream names should be case-sensitive, allowed symbols, etc.
// TODO(dkorolev): Ensure no streams with the same name are being added. Add an exception for it.

template <typename ENTRY, class CLONER = current::DefaultCloner>
using DEFAULT_PERSISTENCE_LAYER = blocks::persistence::MemoryOnly<ENTRY, CLONER>;

template <typename ENTRY, class CLONER = current::DefaultCloner>
StreamInstance<ENTRY, DEFAULT_PERSISTENCE_LAYER, CLONER> Stream(const std::string& name) {
  return StreamInstance<ENTRY, DEFAULT_PERSISTENCE_LAYER, CLONER>(
      new StreamInstanceImpl<ENTRY, DEFAULT_PERSISTENCE_LAYER, CLONER>(name));
}

template <typename ENTRY,
          template <typename, typename> class PERSISTENCE_LAYER,
          class CLONER = current::DefaultCloner,
          typename... EXTRA_PARAMS>
StreamInstance<ENTRY, PERSISTENCE_LAYER, CLONER> Stream(const std::string& name,
                                                        EXTRA_PARAMS&&... extra_params) {
  return StreamInstance<ENTRY, PERSISTENCE_LAYER, CLONER>(
      new StreamInstanceImpl<ENTRY, PERSISTENCE_LAYER, CLONER>(name,
                                                               std::forward<EXTRA_PARAMS>(extra_params)...));
}

}  // namespace sherlock

#endif  // SHERLOCK_H
