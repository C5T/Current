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
#include "stream_data.h"
#include "pubsub.h"

#include "../TypeSystem/struct.h"
#include "../TypeSystem/Schema/schema.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/Persistence/persistence.h"
#include "../Blocks/SS/ss.h"

#include "../Bricks/sync/scope_owned.h"
#include "../Bricks/time/chrono.h"
#include "../Bricks/util/waitable_terminate_signal.h"

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
// Publishing is done via `my_stream.Publish(ENTRY{...});`.
//
// Subscription is done via `auto scope = my_stream.Subscribe(my_subscriber);`, where `my_subscriber`
// is an instance of the class doing the subscription. Sherlock runs each subscriber in a dedicated thread.
//
// Stack ownership of `my_subscriber` is respected, and `SubscriberScope` is returned for the user to store.
// As the returned `scope` object leaves the scope, the subscriber is sent a signal to terminate,
// and the destructor of `scope` waits for the subscriber to do so. The `scope` objects can be `std::move()`-d.
//
// The `my_subscriber` object should be an instance of `StreamSubscriber<IMPL, ENTRY>`,

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

enum class StreamDataAuthority : bool { Own = true, External = false };

template <typename ENTRY>
using DEFAULT_PERSISTENCE_LAYER = current::persistence::Memory<ENTRY>;

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
class StreamImpl {
 public:
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<entry_t>;
  using stream_data_t = StreamData<entry_t, PERSISTENCE_LAYER>;

  class StreamPublisher {
   public:
    explicit StreamPublisher(ScopeOwned<stream_data_t>& data) : data_(data, []() {}) {}

    template <current::locks::MutexLockStatus MLS>
    idxts_t DoPublish(const entry_t& entry, const std::chrono::microseconds us = current::time::Now()) {
      if (!data_) {
        CURRENT_THROW(StreamInGracefulShutdownException());
      }
      current::locks::SmartMutexLockGuard<MLS> lock(data_->publish_mutex);
      const auto result = data_->persistence.Publish(entry, us);
      data_->notifier.NotifyAllOfExternalWaitableEvent();
      return result;
    }

    template <current::locks::MutexLockStatus MLS>
    idxts_t DoPublish(entry_t&& entry, const std::chrono::microseconds us = current::time::Now()) {
      if (!data_) {
        CURRENT_THROW(StreamInGracefulShutdownException());
      }
      current::locks::SmartMutexLockGuard<MLS> lock(data_->publish_mutex);
      const auto result = data_->persistence.Publish(std::move(entry), us);
      data_->notifier.NotifyAllOfExternalWaitableEvent();
      return result;
    }

    operator bool() const { return data_; }

   private:
    ScopeOwnedBySomeoneElse<stream_data_t> data_;
  };
  using publisher_t = ss::StreamPublisher<StreamPublisher, entry_t>;

  template <typename... ARGS>
  StreamImpl(ARGS&&... args)
      : data_(std::forward<ARGS>(args)...),
        publisher_(std::make_unique<publisher_t>(data_)),
        authority_(StreamDataAuthority::Own) {}

  StreamImpl(StreamImpl&& rhs)
      : data_(std::move(rhs.data_)), publisher_(std::move(rhs.publisher_)), authority_(rhs.authority_) {}

  ~StreamImpl() {
    // TODO(dkorolev): These should be erased in an ongoing fashion.
    data_->http_subscriptions.clear();
  }

  void operator=(StreamImpl&& rhs) {
    data_ = std::move(rhs.data_);
    publisher_ = std::move(rhs.publisher_);
    authority_ = rhs.authority_;
  }

  idxts_t Publish(const entry_t& entry, const std::chrono::microseconds us = current::time::Now()) {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    if (publisher_) {
      return publisher_->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(entry, us);
    } else {
      CURRENT_THROW(PublishToStreamWithReleasedPublisherException());
    }
  }

  idxts_t Publish(entry_t&& entry, const std::chrono::microseconds us = current::time::Now()) {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    if (publisher_) {
      return publisher_->template Publish<current::locks::MutexLockStatus::AlreadyLocked>(std::move(entry), us);
    } else {
      CURRENT_THROW(PublishToStreamWithReleasedPublisherException());
    }
  }

  template <typename ACQUIRER>
  void MovePublisherTo(ACQUIRER&& acquirer) {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    if (publisher_) {
      acquirer.AcceptPublisher(std::move(publisher_));
      authority_ = StreamDataAuthority::External;
    } else {
      CURRENT_THROW(PublisherAlreadyReleasedException());
    }
  }

  void AcquirePublisher(std::unique_ptr<publisher_t> publisher) {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    if (!publisher_) {
      publisher_ = std::move(publisher);
      authority_ = StreamDataAuthority::Own;
    } else {
      CURRENT_THROW(PublisherAlreadyOwnedException());
    }
  }

  StreamDataAuthority DataAuthority() const {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    return authority_;
  }

  template <typename TYPE_SUBSCRIBED_TO, typename F>
  class SubscriberThreadInstance final : public current::sherlock::SubscriberScope::SubscriberThread {
   private:
    F& subscriber;
    std::function<void()> done_callback_;
    ScopeOwnedBySomeoneElse<stream_data_t> data;
    std::atomic_bool thread_done_{false};
    current::WaitableTerminateSignal terminate_signal;
    std::thread thread_;

    SubscriberThreadInstance() = delete;
    SubscriberThreadInstance(const SubscriberThreadInstance&) = delete;
    SubscriberThreadInstance(SubscriberThreadInstance&&) = delete;
    void operator=(const SubscriberThreadInstance&) = delete;
    void operator=(SubscriberThreadInstance&&) = delete;

   public:
    SubscriberThreadInstance(ScopeOwned<stream_data_t>& data,
                             F& subscriber,
                             std::function<void()> done_callback)
        : subscriber(subscriber),
          done_callback_(done_callback),
          data(data, [this]() { terminate_signal.SignalExternalTermination(); }),
          thread_(&SubscriberThreadInstance::Thread, this) {}

    ~SubscriberThreadInstance() {
      assert(thread_.joinable());
      if (!thread_done_) {
        terminate_signal.SignalExternalTermination();
      }
      thread_.join();
    }

    void Thread() {
      ThreadImpl();
      thread_done_ = true;
      if (done_callback_) {
        // TODO(dkorolev): Fix this ownership issue.
        std::lock_guard<std::mutex> lock(data->http_subscriptions_mutex);
        done_callback_();
      }
    }

    void ThreadImpl() {
      size_t index = 0;
      size_t size = 0;
      bool terminate_sent = false;
      while (true) {
        // TODO(dkorolev): This `EXCL` section can and should be tested by subscribing to an empty stream.
        // TODO(dkorolev): This is actually more a case of `EndReached()` first, right?
        if (!terminate_sent && terminate_signal) {
          terminate_sent = true;
          if (subscriber.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        size = data->persistence.Size();
        if (size > index) {
          for (const auto& e : data->persistence.Iterate(index, size)) {
            if (!terminate_sent && terminate_signal) {
              terminate_sent = true;
              if (subscriber.Terminate() != ss::TerminationResponse::Wait) {
                return;
              }
            }
            if (current::ss::PassEntryToSubscriberIfTypeMatches<TYPE_SUBSCRIBED_TO, entry_t>(
                    subscriber,
                    [this]() -> ss::EntryResponse { return subscriber.EntryResponseIfNoMorePassTypeFilter(); },
                    e.entry,
                    e.idx_ts,
                    data->persistence.LastPublishedIndexAndTimestamp()) == ss::EntryResponse::Done) {
              return;
            }
            index = size;
          }
        } else {
          std::unique_lock<std::mutex> lock(data->publish_mutex);
          current::WaitableTerminateSignalBulkNotifier::Scope scope(data->notifier, terminate_signal);
          terminate_signal.WaitUntil(
              lock, [this, &index]() { return terminate_signal || data->persistence.Size() > index; });
        }
      }
    }
  };

  // Expose the means to control the scope of the subscriber.
  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t>
  class SubscriberScope : public current::sherlock::SubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using base_t = current::sherlock::SubscriberScope;

   public:
    using subscriber_thread_t = SubscriberThreadInstance<TYPE_SUBSCRIBED_TO, F>;

    SubscriberScope(ScopeOwned<stream_data_t>& data, F& subscriber, std::function<void()> done_callback)
        : base_t(std::move(std::make_unique<subscriber_thread_t>(data, subscriber, done_callback))) {}
  };

  template <typename TYPE_SUBSCRIBED_TO = entry_t, typename F>
  SubscriberScope<F, TYPE_SUBSCRIBED_TO> Subscribe(F& subscriber,
                                                   std::function<void()> done_callback = nullptr) {
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    return SubscriberScope<F, TYPE_SUBSCRIBED_TO>(data_, subscriber, done_callback);
  }

  // Sherlock handler for serving stream data via HTTP (see `pubsub.h` for details).
  template <JSONFormat J = JSONFormat::Current>
  void ServeDataViaHTTP(Request r) {
    if (r.url.query.has("terminate")) {
      const std::string id = r.url.query["terminate"];
      typename stream_data_t::http_subscriptions_t::iterator it;
      {
        // NOTE: This is not thread-safe!!!
        // The iterator may get invalidated in between the next few lines.
        // However, during the call to `= nullptr` the mutex will be locked again from within the
        // thread terminating callback. Need to clean this up. TODO(dkorolev).
        std::lock_guard<std::mutex> lock(data_->http_subscriptions_mutex);
        it = data_->http_subscriptions.find(id);
      }
      if (it != data_->http_subscriptions.end()) {
        // Subscription found. Delete the scope, triggering the thread to shut down.
        // TODO(dkorolev): This should certainly not happen synchronously.
        it->second.first = nullptr;
        // Subscription terminated.
        r("", HTTPResponseCode.OK);
      } else {
        // Subscription not found.
        r("", HTTPResponseCode.NotFound);
      }
      return;
    }
    if (r.method == "GET" || r.method == "HEAD") {
      const size_t count = data_->persistence.Size();
      if (r.method == "HEAD") {
        // Return the number of entries in the stream in `X-Current-Stream-Size` header.
        r("",
          HTTPResponseCode.OK,
          current::net::constants::kDefaultContentType,
          current::net::http::Headers({{kSherlockHeaderCurrentStreamSize, current::ToString(count)}}));
      } else {
        bool schema_requested = false;
        std::string schema_format;
        const std::string schema_prefix = "schema.";
        if (r.url.query.has("schema")) {
          schema_requested = true;
          schema_format = r.url.query["schema"];
        } else if (r.url_path_args.size() == 1) {
          if (r.url_path_args[0].substr(0, schema_prefix.length()) == schema_prefix) {
            schema_requested = true;
            schema_format = r.url_path_args[0].substr(schema_prefix.length());
          } else {
            SherlockSchemaFormatNotFound four_oh_four;
            four_oh_four.unsupported_format_requested = r.url_path_args[0];
            r(four_oh_four, HTTPResponseCode.NotFound);
            return;
          }
        }
        if (schema_requested) {
          // Return the schema the user is requesting, in a top-level, or more fine-grained format.
          if (schema_format.empty()) {
            r(schema_as_http_response_);
          } else {
            const auto cit = schema_as_object_.language.find(schema_format);
            if (cit != schema_as_object_.language.end()) {
              r(cit->second);
            } else {
              SherlockSchemaFormatNotFound four_oh_four;
              four_oh_four.unsupported_format_requested = schema_format;
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
          const std::string subscription_id = data_->GenerateRandomHTTPSubscriptionID();

          auto http_chunked_subscriber = std::make_unique<PubSubHTTPEndpoint<entry_t, PERSISTENCE_LAYER, J>>(
              subscription_id, data_, std::move(r));

          current::sherlock::SubscriberScope http_chunked_subscriber_scope =
              Subscribe(*http_chunked_subscriber,
                        [this, subscription_id]() {
                          // NOTE: Need to figure out when and where to lock.
                          // Chat w/ Max about the logic to clean up completed listeners.
                          // std::lock_guard<std::mutex> lock(inner_data.http_subscriptions_mutex);
                          data_->http_subscriptions[subscription_id].second = nullptr;
                        });

          {
            std::lock_guard<std::mutex> lock(data_->http_subscriptions_mutex);
            assert(!data_->http_subscriptions.count(subscription_id));
            data_->http_subscriptions[subscription_id] =
                std::make_pair(std::move(http_chunked_subscriber_scope), std::move(http_chunked_subscriber));
          }
        }
      }
    } else {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
    }
  }

  void operator()(Request r) { ServeDataViaHTTP(std::move(r)); }

  persistence_layer_t& InternalExposePersister() { return data_->persistence; }

 private:
  ScopeOwnedByMe<stream_data_t> data_;
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
  const Response schema_as_http_response_ = Response(JSON<JSONFormat::Minimalistic>(schema_as_object_),
                                                     HTTPResponseCode.OK,
                                                     current::net::constants::kDefaultJSONContentType);

  mutable std::mutex publisher_mutex_;
  std::unique_ptr<publisher_t> publisher_;
  StreamDataAuthority authority_;

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
