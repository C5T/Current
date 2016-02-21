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

#ifndef CURRENT_SHERLOCK_SHERLOCK_H
#define CURRENT_SHERLOCK_SHERLOCK_H

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
#include "../Bricks/util/null_deleter.h"
#include "../Bricks/waitable_atomic/waitable_atomic.h"
#include "../Bricks/util/waitable_terminate_signal.h"

// Sherlock is the overlord of streamed data storage and processing in Current.
// Sherlock's streams are persistent, immutable, append-only typed sequences of records ("entries").
// Each record is annotated with its the 1-based index and its epoch microsecond timestamp.
// Within the stream, timestamps are strictly increasing.
//
// A stream can be constucted as `auto my_stream = sherlock::Stream<ENTRY>()`. This creates an in-memory stream.

// To create a persisted one, pass in the type of persister and its construction parameters, such as:
// `auto my_stream = sherlock::Stream<ENTRY, current::persistence::File>("data.json");`.
//
// Sherlock streams can be published into and subscribed to.
//
// Publishing is done via `my_stream.Publish(ENTRY{...});`. It is the caller's responsibility to ensure that:
// 1) Publishing is done from one thread only (Sherlock itself offers no locking), and
// 2) Published entries come in strictly increasing order of their timestamps.
//
// Subscription is done by via `my_stream.Subscribe(my_listener);`, where `my_listener` is an instance
// of the class doing the listening. Sherlock runs each listener in a dedicated thread.
//
// The parameter to `Subscribe()` can be an `std::unique_ptr<F>`, or a reference to a stack-allocated object.
// In the first case, subscriber thread takes ownership of the listener, and returns an `AsyncListenerScope`.
// In the second case, stack ownership is respected, and `SyncListenerScope` is returned.
// Both scopes can be `.Join()`-ed, which would be a blocking call waiting until the listener is done.
// Asynchronous scope can also be `.Detach()`-ed, internally calling `std::thread::detach()`.
// A detached listener will live until it decides to terminate. In case the stream itself would be destructing,
// each listener, detached listeners included, will be notified of stream termination, and the destructor
// of the stream objectwill wait for all listeners, detached included, to terminate themselves.
//
// The `my_listener` object should expose the following member functions:
//
// 1) `bool operator()({const ENTRY& / ENTRY&&} entry, IndexAndTimestamp current, IndexAndTimestamp last))`:
//
//    The `ENTRY` type is RTTI-dispatched against the supplied type list.
//    As long as `my_listener` returns `true`, it will keep receiving new entries,
//    which may end up blocking the thread until new, yet unseen, entries have been published.
//
//    If `operator()` is defined as `bool`, returning `false` will tell Sherlock that no more entries
//    should be passed to this listener, and the thread serving this listener should be terminated.
//
//    More details on the calling convention: "Bricks/mq/interface/interface.h"
//
// 2) `bool Terminate()`:
//
//    This member function will be called if the listener has to be terminated externally,
//    which happens when the handler returned by `Subscribe()` goes out of scope.
//    If the signature is `bool Terminate()`, returning `false` will prevent the termination from happening,
//    allowing the user code to process more entries. Note that this can yield to the calling thread
//    waiting indefinitely.
//
// 3) `HeadUpdated()`: TBD.
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

namespace current {
namespace sherlock {

template <typename ENTRY>
using DEFAULT_PERSISTENCE_LAYER = current::persistence::Memory<ENTRY>;

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
class StreamImpl {
  using IDX_TS = current::ss::IndexAndTimestamp;

 public:
  using T_ENTRY = ENTRY;
  using T_PERSISTENCE_LAYER = PERSISTENCE_LAYER<ENTRY>;

  StreamImpl()
      : storage_(std::make_shared<T_PERSISTENCE_LAYER>()),
        last_idx_ts_(std::make_shared<current::WaitableAtomic<IDX_TS>>()) {}

  template <typename... EXTRA_PARAMS>
  StreamImpl(EXTRA_PARAMS&&... extra_params)
      : storage_(std::make_shared<T_PERSISTENCE_LAYER>(std::forward<EXTRA_PARAMS>(extra_params)...)),
        last_idx_ts_(std::make_shared<current::WaitableAtomic<IDX_TS>>()) {}

  StreamImpl(StreamImpl&& rhs) : storage_(std::move(rhs.storage_)), last_idx_ts_(std::move(rhs.last_idx_ts_)) {}

  void operator=(StreamImpl&& rhs) {
    storage_ = std::move(rhs.storage_);
    last_idx_ts_(std::move(rhs.last_idx_ts_));
  }

  // `Publish()` and `Emplace()` return the index and the timestamp of the added entry.
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  IDX_TS Publish(const ENTRY& entry) {
    const IDX_TS result = storage_->Publish(entry);
    last_idx_ts_->SetValue(result);
    return result;
  }
  IDX_TS Publish(ENTRY&& entry) {
    const IDX_TS result = storage_->Publish(std::move(entry));
    last_idx_ts_->SetValue(result);
    return result;
  }

  template <typename... ARGS>
  IDX_TS Emplace(ARGS&&... entry_params) {
    const IDX_TS result = storage_->Emplace(std::forward<ARGS>(entry_params)...);
    last_idx_ts_->SetValue(result);
    return result;
  }

  // `PublishReplayed()` is used for replication.
  void PublishReplayed(const ENTRY& entry, IDX_TS idx_ts) {
    storage_->PublishReplayed(entry, idx_ts);
    last_idx_ts_->SetValue(idx_ts);
  }
  void PublishReplayed(ENTRY&& entry, IDX_TS idx_ts) {
    storage_->PublishReplayed(std::move(entry), idx_ts);
    last_idx_ts_->SetValue(idx_ts);
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
    // Listener thread shared state is a `shared_ptr<>` itself, to ensure its lifetime.
    struct ListenerThreadSharedState {
      F listener;  // The ownership of the listener is transferred to instance of this class.
      std::shared_ptr<T_PERSISTENCE_LAYER> persister;
      std::shared_ptr<current::WaitableAtomic<IDX_TS>> last_idx_ts;

      current::WaitableTerminateSignal terminate_signal;

      ListenerThreadSharedState(std::shared_ptr<T_PERSISTENCE_LAYER> persister,
                                std::shared_ptr<current::WaitableAtomic<IDX_TS>> last_idx_ts,
                                F&& listener)
          : listener(std::move(listener)), persister(persister), last_idx_ts(last_idx_ts) {}

      ListenerThreadSharedState() = delete;
      ListenerThreadSharedState(const ListenerThreadSharedState&) = delete;
      ListenerThreadSharedState(ListenerThreadSharedState&&) = delete;
      void operator=(const ListenerThreadSharedState&) = delete;
      void operator=(ListenerThreadSharedState&&) = delete;
    };

   public:
    ListenerThread(std::shared_ptr<T_PERSISTENCE_LAYER> persister,
                   std::shared_ptr<current::WaitableAtomic<IDX_TS>> last_idx_ts,
                   F&& listener)
        : state_(
              std::make_shared<ListenerThreadSharedState>(persister, last_idx_ts, std::forward<F>(listener))),
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
      size_t index = 0;
      size_t size = 0;
      bool terminate_sent = false;
      while (true) {
        if (!terminate_sent && state->terminate_signal) {
          terminate_sent = true;
          if ((*state->listener).Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        size = state->persister->Size();
        if (size > index) {
          for (const auto& e : state->persister->Iterate(index, size)) {
            if (!terminate_sent && state->terminate_signal) {
              terminate_sent = true;
              if ((*state->listener).Terminate() != ss::TerminationResponse::Wait) {
                return;
              }
            }
            if ((*state->listener)(e.entry, e.idx_ts, state->last_idx_ts->GetValue()) ==
                ss::EntryResponse::Done) {
              return;
            }
            index = size;
          }
        } else {
          ;  // Spin lock, to be replaced by the proper waiting logic. -- D.K.
        }
      }
    }

    std::shared_ptr<ListenerThreadSharedState> state_;
    std::thread thread_;

    ListenerThread(ListenerThread&&) = delete;  // Yep -- no move constructor.

    ListenerThread() = delete;
    ListenerThread(const ListenerThread&) = delete;
    void operator=(const ListenerThread&) = delete;
    void operator=(ListenerThread&&) = delete;
  };

  // Expose the means to create both a sync ("scoped") and async ("detachable") listeners.
  template <class F, class UNIQUE_PTR_WITH_CORRECT_DELETER, bool CAN_DETACH>
  class GenericListenerScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, ENTRY>::value, "");
    using LISTENER_THREAD = ListenerThread<UNIQUE_PTR_WITH_CORRECT_DELETER>;

   public:
    GenericListenerScope(std::shared_ptr<T_PERSISTENCE_LAYER> persister,
                         std::shared_ptr<current::WaitableAtomic<IDX_TS>> last_idx_ts,
                         UNIQUE_PTR_WITH_CORRECT_DELETER&& listener)
        : impl_(std::make_unique<LISTENER_THREAD>(persister, last_idx_ts, std::move(listener))) {}

    GenericListenerScope(GenericListenerScope&& rhs) : impl_(std::move(rhs.impl_)) {
      assert(impl_);
      assert(!rhs.impl_);
    }

    void Join() {
      assert(impl_);
      impl_->SafeJoinThread();
    }

    template <bool B = CAN_DETACH>
    ENABLE_IF<B> Detach() {
      assert(impl_);
      impl_->PotentiallyUnsafeDetachThread();
    }

   private:
    std::unique_ptr<LISTENER_THREAD> impl_;

    GenericListenerScope() = delete;
    GenericListenerScope(const GenericListenerScope&) = delete;
    void operator=(const GenericListenerScope&) = delete;
    void operator=(GenericListenerScope&&) = delete;
  };

  // Asynchronous subscription: `listener` is a heap-allocated object, the ownership of which
  // can be `std::move()`-d into the listening thread. It can be `Join()`-ed or `Detach()`-ed.
  template <typename F>
  using AsyncListenerScope = GenericListenerScope<F, std::unique_ptr<F>, true>;

  template <typename F>
  AsyncListenerScope<F> Subscribe(std::unique_ptr<F>&& listener) {
    static_assert(current::ss::IsStreamSubscriber<F, ENTRY>::value, "");
    return std::move(AsyncListenerScope<F>(storage_, last_idx_ts_, std::move(listener)));
  }

  // Synchronous subscription: `listener` is a stack-allocated object, and thus the listening thread
  // should ensure to terminate itself, when initiated from within the destructor of `SyncListenerScope`.
  // Note that the destructor of `SyncListenerScope` will wait until the listener terminates, thus,
  // not terminating as requested may result in the calling thread blocking for an unbounded amount of time.
  template <typename F>
  using SyncListenerScope = GenericListenerScope<F, std::unique_ptr<F, current::NullDeleter>, false>;

  template <typename F>
  SyncListenerScope<F> Subscribe(F& listener) {
    static_assert(current::ss::IsStreamSubscriber<F, ENTRY>::value, "");
    return std::move(
        SyncListenerScope<F>(storage_, last_idx_ts_, std::unique_ptr<F, current::NullDeleter>(&listener)));
  }

  // Sherlock handler for serving stream data via HTTP.
  // Expects "GET" request with the following possible parameters:
  // * `recent={us}` to get entries within `us` microseconds from now;
  // * `since={us_timestamp}` to get entries since the specified timestamp;
  // * `n={x}` to get last `x` entries;
  // * `n_min={min}` to get at least `min` entries;
  // * `cap={max}` to get at most `max` entries;
  // * `nowait` to stop serving when the last entry in the stream reached;
  // * `sizeonly` to get the current number of entries in the stream instead of its content.
  // See `pubsub.h` for details.
  template <JSONFormat J = JSONFormat::Current>
  void ServeDataViaHTTP(Request r) {
    if (r.method == "GET") {  // The only valid method is "GET".
      const size_t count = storage_->Size();
      if (r.url.query.has("sizeonly")) {
        // Return the number of entries in the stream.
        r(ToString(count) + '\n', HTTPResponseCode.OK);
      } else if (count == 0u && r.url.query.has("nowait")) {
        // Return "200 OK" if stream is empty and we were asked to not wait for new entries.
        r("", HTTPResponseCode.OK);
      } else {
        Subscribe(std::make_unique<PubSubHTTPEndpoint<ENTRY, J>>(std::move(r))).Detach();
      }
    } else {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
    }
  }

  // TODO(dkorolev): Have this co-own the Stream.
  void operator()(Request r) { ServeDataViaHTTP(std::move(r)); }

  T_PERSISTENCE_LAYER& InternalExposePersister() { return *storage_; }

 private:
  std::shared_ptr<T_PERSISTENCE_LAYER> storage_;
  std::shared_ptr<current::WaitableAtomic<IDX_TS>> last_idx_ts_;

  StreamImpl(const StreamImpl&) = delete;
  void operator=(const StreamImpl&) = delete;
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
using Stream = StreamImpl<ENTRY, PERSISTENCE_LAYER>;

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_SHERLOCK_H
