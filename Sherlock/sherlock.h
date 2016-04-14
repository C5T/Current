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
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "exceptions.h"
#include "pubsub.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/Schema/schema.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/Persistence/persistence.h"
#include "../Blocks/SS/ss.h"

#include "../Bricks/template/decay.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/util/null_deleter.h"
#include "../Bricks/util/waitable_terminate_signal.h"

// TODO(dk+mz): Revisit all the comments here.
// Sherlock is the overlord of streamed data storage and processing in Current.
// Sherlock's streams are persistent, immutable, append-only typed sequences of records ("entries").
// Each record is annotated with its the 1-based index and its epoch microsecond timestamp.
// Within the stream, timestamps are strictly increasing.
//
// A stream is constructed as `auto my_stream = sherlock::Stream<ENTRY>()`. This creates an in-memory stream.

// To create a persisted one, pass in the type of persister and its construction parameters, such as:
// `auto my_stream = sherlock::Stream<ENTRY, current::persistence::File>("data.json");`.
//
// Sherlock streams can be published into and subscribed to.
//
// Publishing is done via `my_stream.Publish(ENTRY{...});`. It is the caller's responsibility
// to ensure publishing is done from one thread only (Sherlock itself offers no locking).
//
// Subscription is done via `my_stream.Subscribe(my_subscriber);`, where `my_subscriber` is an instance
// of the class doing the subscription. Sherlock runs each subscriber in a dedicated thread.
//
// The parameter to `Subscribe()` can be an `std::unique_ptr<F>`, or a reference to a stack-allocated object.
// In the first case, subscriber thread takes ownership of the subscriber, and returns an
// `AsyncSubscriberScope`.
// In the second case, stack ownership is respected, and `SyncSubscriberScope` is returned.
// Both scopes can be `.Join()`-ed, which would be a blocking call waiting until the subscriber is done.
// Asynchronous scope can also be `.Detach()`-ed, internally calling `std::thread::detach()`.
// A detached subscriber will live until it decides to terminate. In case the stream itself would be
// destructing, each subscriber, detached subscribers included, will be notified of stream termination,
// and the destructor of the stream object will wait for all subscribers, detached included, to terminate
// themselves.
//
// The `my_subscriber` object should be an instance of `StreamSubscriber<IMPL, ENTRY>`,
// and `IMPL should implement the following methods with respective return values.
//
// 1) `bool operator()({const ENTRY& / ENTRY&&} entry, IndexAndTimestamp current, IndexAndTimestamp last))`:
//
//    The `ENTRY` type is RTTI-dispatched against the supplied type list.
//    As long as `my_subscriber` returns `true`, it will keep receiving new entries,
//    which may end up blocking the thread until new, yet unseen, entries have been published.
//
//    If `operator()` is defined as `bool`, returning `false` will tell Sherlock that no more entries
//    should be passed to this subscriber, and the thread serving this subscriber should be terminated.
//
//    More details on the calling convention: "Bricks/mq/interface/interface.h"
//
// 2) `bool Terminate()`:
//
//    This member function will be called if the subscriber has to be terminated externally,
//    which happens when the handler returned by `Subscribe()` goes out of scope.
//    If the signature is `bool Terminate()`, returning `false` will prevent the termination from happening,
//    allowing the user code to process more entries. Note that this can yield to the calling thread
//    waiting indefinitely.
//
// 3) `HeadUpdated()`: TBD.
//
// The call to `my_stream.Subscribe(my_subscriber);` launches the subscriber and returns
// an instance of a handle, the scope of which will define the lifetime of the subscriber.
//
// If the subscribing thread would like the subscriber to run forever, it can use `.Join()` or `.Detach()`
// on the handle. `Join()` will block the calling thread unconditionally, until the subscriber itself
// decides to stop functioning. `Detach()` will ensure that the ownership of the subscriber object
// has been transferred to the thread running the subscriber, and detach this thread to run in the background.
//
// TODO(dkorolev): Add timestamps support and tests.
// TODO(dkorolev): Ensure the timestamps always come in a non-decreasing order.

namespace current {
namespace sherlock {

CURRENT_STRUCT(SherlockSchema) {
  CURRENT_FIELD(language, (std::map<std::string, std::string>));
  CURRENT_FIELD(type_name, std::string);
  CURRENT_FIELD(type_id, current::reflection::TypeID);
  CURRENT_FIELD(type_schema, reflection::SchemaInfo);
  CURRENT_DEFAULT_CONSTRUCTOR(SherlockSchema) {}
};

CURRENT_STRUCT(SherlockSchemaFormatNotFound) {
  CURRENT_FIELD(error, std::string, "Unsupported schema format requested.");
  CURRENT_FIELD(unsupported_format_requested, Optional<std::string>);
};

template <typename ENTRY>
using DEFAULT_PERSISTENCE_LAYER = current::persistence::Memory<ENTRY>;

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
class StreamImpl {
 public:
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<ENTRY>;

  // The inner structure containing all the data required for the stream to function.
  // TODO(dkorolev): Make it a `ScopeOwnedByMe` type of thing.
  struct StreamData {
    persistence_layer_t persistence;

    current::WaitableTerminateSignalBulkNotifier notifier;
    std::mutex mutex;

    template <typename... ARGS>
    StreamData(ARGS&&... args)
        : persistence(std::forward<ARGS>(args)...) {}
  };

  class StreamPublisher {
   public:
    explicit StreamPublisher(std::shared_ptr<StreamData> data) : data_(data) {}

    template <current::locks::MutexLockStatus MLS>
    idxts_t DoPublish(const ENTRY& entry, const std::chrono::microseconds us = current::time::Now()) {
      current::locks::SmartMutexLockGuard<MLS> lock(data_->mutex);
      const auto result = data_->persistence.Publish(entry, us);
      data_->notifier.NotifyAllOfExternalWaitableEvent();
      return result;
    }

    template <current::locks::MutexLockStatus MLS>
    idxts_t DoPublish(ENTRY&& entry, const std::chrono::microseconds us = current::time::Now()) {
      current::locks::SmartMutexLockGuard<MLS> lock(data_->mutex);
      const auto result = data_->persistence.Publish(std::move(entry), us);
      data_->notifier.NotifyAllOfExternalWaitableEvent();
      return result;
    }

   private:
    std::shared_ptr<StreamData> data_;
  };
  using publisher_t = ss::StreamPublisher<StreamPublisher, ENTRY>;

  template <typename... ARGS>
  StreamImpl(ARGS&&... args)
      : data_(std::make_shared<StreamData>(std::forward<ARGS>(args)...)),
        publisher_(std::make_unique<publisher_t>(data_)) {}

  StreamImpl(StreamImpl&& rhs) : data_(std::move(rhs.data_)), publisher_(std::move(rhs.publisher_)) {}

  void operator=(StreamImpl&& rhs) {
    data_ = std::move(rhs.data_);
    publisher_ = std::move(rhs.publisher_);
  }

  // `Publish()` and `Emplace()` return the index and the timestamp of the added entry.
  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  // TODO(dkorolev) + TODO(mzhurovich): Shoudn't these be `DoPublish()`?
  // template <typename... ARGS>
  // idxts_t DoEmplace(ARGS&&... entry_params) {
  //   std::lock_guard<std::mutex> lock(data_->mutex);
  //   const auto result = data_->persistence.Emplace(std::forward<ARGS>(entry_params)...);
  //   data_->notifier.NotifyAllOfExternalWaitableEvent();
  //   return result;
  // }

  // `SubscriberThread` spawns the thread and runs stream subscriber within it.
  //
  // Subscriber thread can always be `std::thread::join()`-ed. When this happens, the subscriber itself is
  // notified
  // that the user has requested to join the subscriber thread, and then the thread is joined.
  // The subscriber code itself may choose to stop immediately, later, or never; in the latter cases
  // `join()`-ing
  // the thread will run for a long time or forever.
  // Most commonly though, `join()` on a subscriber thread will result it in completing processing the entry
  // that it is currently processing, and then shutting down gracefully.
  // This makes working with C++ Sherlock almost as cozy as it could have been in node.js or Python.
  //
  // With respect to `std::thread::detach()`, we have two very different usecases:
  //
  // 1) If `SubscriberThread` has been provided with a fair `unique_ptr<>` (which obviously is immediately
  //    `std::move()`-d into the thread, thus enabling `thread::detach()`), then `thread::detach()`
  //    is a legal operation, and it is the way to detach the subscriber from the caller thread,
  //    enabling to run the subscriber indefinitely, or until it itself decides to stop.
  //    The most notable example here would be spawning a subscriber to serve an HTTP request.
  //    (NOTE: This logic requires `Stream` objects to live forever. TODO(dk+mz): Make sure it is the case.)
  //
  // 2) The alternate usecase is when a stack-allocated object should acts as a subscriber.
  //    Implementation-wise, it is handled by wrapping the stack-allocated object into a `unique_ptr<>`
  //    with null deleter. Obviously, `thread::detach()` is then unsafe and thus impossible.
  //
  // The `SubscriberScope` class handles the above two usecases, depending on whether a stack- or heap-allocated
  // instance of a subscriber has been passed into.

  idxts_t Publish(const ENTRY& entry, const std::chrono::microseconds us = current::time::Now()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (publisher_) {
      return publisher_->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(entry, us);
    } else {
      CURRENT_THROW(PublishToStreamWithReleasedPublisherException());
    }
  }

  idxts_t Publish(ENTRY&& entry, const std::chrono::microseconds us = current::time::Now()) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (publisher_) {
      return publisher_->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(std::move(entry), us);
    } else {
      CURRENT_THROW(PublishToStreamWithReleasedPublisherException());
    }
  }

  template <typename ACQUIRER>
  void MovePublisherTo(ACQUIRER&& acquirer) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (publisher_) {
      acquirer.AcceptPublisher(std::move(publisher_));
    } else {
      CURRENT_THROW(PublisherAlreadyReleasedException());
    }
  }

  void AcquirePublisher(std::unique_ptr<publisher_t> publisher) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!publisher_) {
      publisher_ = std::move(publisher);
    } else {
      CURRENT_THROW(PublisherAlreadyOwnedException());
    }
  }

  template <typename TYPE_SUBSCRIBED_TO, typename F>
  class SubscriberThread {
   private:
    // Subscriber thread shared state is a `shared_ptr<>` itself, to ensure its lifetime.
    struct SubscriberThreadSharedState {
      F subscriber;  // The ownership of the subscriber is transferred to instance of this class.
      std::shared_ptr<StreamData> data;
      current::WaitableTerminateSignal terminate_signal;

      SubscriberThreadSharedState(std::shared_ptr<StreamData> data, F&& subscriber)
          : subscriber(std::move(subscriber)), data(data) {}

      SubscriberThreadSharedState() = delete;
      SubscriberThreadSharedState(const SubscriberThreadSharedState&) = delete;
      SubscriberThreadSharedState(SubscriberThreadSharedState&&) = delete;
      void operator=(const SubscriberThreadSharedState&) = delete;
      void operator=(SubscriberThreadSharedState&&) = delete;
    };

   public:
    SubscriberThread(std::shared_ptr<StreamData> data, F&& subscriber)
        : state_(std::make_shared<SubscriberThreadSharedState>(data, std::forward<F>(subscriber))),
          thread_(&SubscriberThread::StaticSubscriberThread, state_) {}

    ~SubscriberThread() {
      if (thread_.joinable()) {
        // TODO(dkorolev): Phrase the error message better.
        // LCOV_EXCL_START
        std::cerr << "Unrecoverable error in destructor: SubscriberThread was not joined/detached.\n";
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

    static void StaticSubscriberThread(std::shared_ptr<SubscriberThreadSharedState> state) {
      auto& subscriber = *state->subscriber;
      size_t index = 0;
      size_t size = 0;
      bool terminate_sent = false;
      while (true) {
        // TODO(dkorolev): This `EXCL` section can and should be tested by subscribing to an empty stream.
        if (!terminate_sent && state->terminate_signal) {
          terminate_sent = true;
          if (subscriber.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        size = state->data->persistence.Size();
        if (size > index) {
          for (const auto& e : state->data->persistence.Iterate(index, size)) {
            if (!terminate_sent && state->terminate_signal) {
              terminate_sent = true;
              if (subscriber.Terminate() != ss::TerminationResponse::Wait) {
                return;
              }
            }
            if (current::ss::PassEntryToSubscriberIfTypeMatches<TYPE_SUBSCRIBED_TO, ENTRY>(
                    subscriber,
                    [&subscriber]()
                        -> ss::EntryResponse { return subscriber.EntryResponseIfNoMorePassTypeFilter(); },
                    e.entry,
                    e.idx_ts,
                    state->data->persistence.LastPublishedIndexAndTimestamp()) == ss::EntryResponse::Done) {
              return;
            }
            index = size;
          }
        } else {
          std::unique_lock<std::mutex> lock(state->data->mutex);
          current::WaitableTerminateSignalBulkNotifier::Scope scope(state->data->notifier,
                                                                    state->terminate_signal);
          state->terminate_signal.WaitUntil(lock,
                                            [&state, &index]() {
                                              return state->terminate_signal ||
                                                     state->data->persistence.Size() > index;
                                            });
        }
      }
    }

    std::shared_ptr<SubscriberThreadSharedState> state_;
    std::thread thread_;

    SubscriberThread(SubscriberThread&&) = delete;  // Yep -- no move constructor.

    SubscriberThread() = delete;
    SubscriberThread(const SubscriberThread&) = delete;
    void operator=(const SubscriberThread&) = delete;
    void operator=(SubscriberThread&&) = delete;
  };

  // Expose the means to create both a sync ("scoped") and async ("detachable") subscribers.
  template <typename F, typename TYPE_SUBSCRIBED_TO, class UNIQUE_PTR_WITH_CORRECT_DELETER, bool CAN_DETACH>
  class GenericSubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using LISTENER_THREAD = SubscriberThread<TYPE_SUBSCRIBED_TO, UNIQUE_PTR_WITH_CORRECT_DELETER>;

   public:
    GenericSubscriberScope(std::shared_ptr<StreamData> data, UNIQUE_PTR_WITH_CORRECT_DELETER&& subscriber)
        : impl_(std::make_unique<LISTENER_THREAD>(data, std::move(subscriber))) {}

    GenericSubscriberScope(GenericSubscriberScope&& rhs) : impl_(std::move(rhs.impl_)) {
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

    GenericSubscriberScope() = delete;
    GenericSubscriberScope(const GenericSubscriberScope&) = delete;
    void operator=(const GenericSubscriberScope&) = delete;
    void operator=(GenericSubscriberScope&&) = delete;
  };

  // Asynchronous subscription: `subscriber` is a heap-allocated object, the ownership of which
  // can be `std::move()`-d into the subscriber thread. It can be `Join()`-ed or `Detach()`-ed.
  // The order of two template parameters is flipped to provide the default value for `TYPE_SUBSCRIBED_TO`,
  // in case the user chooses to specialize `{Async/Sync}SubscriberScope<SINGLE_TEMPLATE_PARAMETER>` directly.
  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t>
  using AsyncSubscriberScope = GenericSubscriberScope<F, TYPE_SUBSCRIBED_TO, std::unique_ptr<F>, true>;

  template <typename TYPE_SUBSCRIBED_TO = entry_t, typename F>
  AsyncSubscriberScope<F, TYPE_SUBSCRIBED_TO> Subscribe(std::unique_ptr<F>&& subscriber) {
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    return AsyncSubscriberScope<F, TYPE_SUBSCRIBED_TO>(data_, std::move(subscriber));
  }

  // Synchronous subscription: `subscriber` is a stack-allocated object, and thus the subscriber thread
  // should ensure to terminate itself, when initiated from within the destructor of `SyncSubscriberScope`.
  // Note that the destructor of `SyncSubscriberScope` will wait until the subscriber terminates, thus,
  // not terminating as requested may result in the calling thread blocking for an unbounded amount of time.
  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t>
  using SyncSubscriberScope =
      GenericSubscriberScope<F, TYPE_SUBSCRIBED_TO, std::unique_ptr<F, current::NullDeleter>, false>;

  template <typename TYPE_SUBSCRIBED_TO = entry_t, typename F>
  SyncSubscriberScope<F, TYPE_SUBSCRIBED_TO> Subscribe(F& subscriber) {
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    return SyncSubscriberScope<F, TYPE_SUBSCRIBED_TO>(data_,
                                                      std::unique_ptr<F, current::NullDeleter>(&subscriber));
  }

  // Sherlock handler for serving stream data via HTTP (see `pubsub.h` for details).
  template <JSONFormat J = JSONFormat::Current>
  void ServeDataViaHTTP(Request r) {
    if (r.method == "GET" || r.method == "HEAD") {
      const size_t count = data_->persistence.Size();
      if (r.method == "HEAD") {
        // Return the number of entries in the stream in `X-Current-Stream-Size` header.
        r("",
          HTTPResponseCode.OK,
          "text/html",
          current::net::http::Headers({{"X-Current-Stream-Size", current::ToString(count)}}));
      } else if (r.url.query.has("schema")) {
        // Return the schema the user is requesting, in a top-level, or more fine-grained format.
        const std::string format = r.url.query["schema"];
        if (format.empty()) {
          r(schema_as_http_response_);
        } else {
          const auto cit = schema_as_object_.language.find(format);
          if (cit != schema_as_object_.language.end()) {
            r(cit->second);
          } else {
            SherlockSchemaFormatNotFound four_oh_four;
            four_oh_four.unsupported_format_requested = format;
            r(four_oh_four, HTTPResponseCode.NotFound);
          }
        }
      } else if (r.url.query.has("sizeonly")) {
        // Return the number of entries in the stream in body.
        r(current::ToString(count) + '\n', HTTPResponseCode.OK);
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

  persistence_layer_t& InternalExposePersister() { return data_->persistence; }

 private:
  struct FillPerLanguageSchema {
    SherlockSchema& schema_ref;
    explicit FillPerLanguageSchema(SherlockSchema& schema) : schema_ref(schema) {}
    template <current::reflection::Language language>
    void PerLanguage() {
      schema_ref.language[current::ToString(language)] = schema_ref.type_schema.Describe<language>();
    }
  };
  static SherlockSchema ConstructSchemaAsObject() {
    SherlockSchema schema;

    schema.type_name = current::reflection::CurrentTypeName<entry_t>();
    schema.type_id = Value<current::reflection::ReflectedTypeBase>(
                         current::reflection::Reflector().ReflectType<entry_t>()).type_id;

    reflection::StructSchema underlying_type_schema;
    underlying_type_schema.AddType<entry_t>();
    schema.type_schema = underlying_type_schema.GetSchemaInfo();

    current::reflection::ForEachLanguage(FillPerLanguageSchema(schema));

    return schema;
  }

 private:
  const SherlockSchema schema_as_object_ = ConstructSchemaAsObject();
  const Response schema_as_http_response_ =
      Response(JSON<JSONFormat::Minimalistic>(schema_as_object_),
               HTTPResponseCode.OK,
               current::net::HTTPServerConnection::DefaultJSONContentType());
  std::shared_ptr<StreamData> data_;
  std::unique_ptr<publisher_t> publisher_;
  std::mutex mutex_;

  StreamImpl(const StreamImpl&) = delete;
  void operator=(const StreamImpl&) = delete;
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
using Stream = StreamImpl<ENTRY, PERSISTENCE_LAYER>;

// TODO(dkorolev) + TODO(mzhurovich): Shouldn't this be:
// using Stream = ss::StreamPublisher<StreamImpl<ENTRY, PERSISTENCE_LAYER>, ENTRY>;

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_SHERLOCK_H
