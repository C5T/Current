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

namespace constants {

constexpr const char* kDefaultNamespaceName = "SherlockSchema";
constexpr const char* kDefaultTopLevelName = "TopLevelTransaction";

}  // namespace constants

CURRENT_STRUCT(SherlockSchema) {
  CURRENT_FIELD(language, (std::map<std::string, std::string>));
  CURRENT_FIELD(type_name, std::string);
  CURRENT_FIELD(type_id, current::reflection::TypeID);
  CURRENT_FIELD(type_schema, reflection::SchemaInfo);
  CURRENT_DEFAULT_CONSTRUCTOR(SherlockSchema) {}
};

CURRENT_STRUCT(SubscribableSherlockSchema) {
  CURRENT_FIELD(type_id, current::reflection::TypeID, current::reflection::TypeID::UninitializedType);
  CURRENT_FIELD(top_level_name, std::string);
  CURRENT_FIELD(namespace_name, std::string);
  CURRENT_DEFAULT_CONSTRUCTOR(SubscribableSherlockSchema) {}
  CURRENT_CONSTRUCTOR(SubscribableSherlockSchema)(
      current::reflection::TypeID type_id, const std::string& top_level_name, const std::string& namespace_name)
      : type_id(type_id), top_level_name(top_level_name), namespace_name(namespace_name) {}
  bool operator==(const SubscribableSherlockSchema& rhs) const {
    return type_id == rhs.type_id && namespace_name == rhs.namespace_name && top_level_name == rhs.top_level_name;
  }
  bool operator!=(const SubscribableSherlockSchema& rhs) const { return !operator==(rhs); }
};

CURRENT_STRUCT(SherlockSchemaFormatNotFound) {
  CURRENT_FIELD(error, std::string, "Unsupported schema format requested.");
  CURRENT_FIELD(unsupported_format_requested, Optional<std::string>);
};

enum class StreamDataAuthority : bool { Own = true, External = false };

template <typename ENTRY>
using DEFAULT_PERSISTENCE_LAYER = current::persistence::Memory<ENTRY>;

struct SherlockNamespaceName {
  const std::string namespace_name;
  const std::string top_level_name;
  explicit SherlockNamespaceName(const std::string& namespace_name, const std::string& top_level_name)
      : namespace_name(namespace_name), top_level_name(top_level_name) {}
};

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
      try {
        auto& data = *data_;
        current::locks::SmartMutexLockGuard<MLS> lock(data.publish_mutex);
        const auto result = data.persistence.Publish(entry, us);
        data.notifier.NotifyAllOfExternalWaitableEvent();
        return result;
      } catch (const current::sync::InDestructingModeException&) {
        CURRENT_THROW(StreamInGracefulShutdownException());
      }
    }

    template <current::locks::MutexLockStatus MLS>
    idxts_t DoPublish(entry_t&& entry, const std::chrono::microseconds us = current::time::Now()) {
      try {
        auto& data = *data_;
        current::locks::SmartMutexLockGuard<MLS> lock(data.publish_mutex);
        const auto result = data.persistence.Publish(std::move(entry), us);
        data.notifier.NotifyAllOfExternalWaitableEvent();
        return result;
      } catch (const current::sync::InDestructingModeException&) {
        CURRENT_THROW(StreamInGracefulShutdownException());
      }
    }

    template <current::locks::MutexLockStatus MLS>
    void DoUpdateHead(const std::chrono::microseconds us = current::time::Now()) {
      try {
        auto& data = *data_;
        current::locks::SmartMutexLockGuard<MLS> lock(data.publish_mutex);
        data.persistence.UpdateHead(us);
        data.notifier.NotifyAllOfExternalWaitableEvent();
      } catch (const current::sync::InDestructingModeException&) {
        CURRENT_THROW(StreamInGracefulShutdownException());
      }
    }

    operator bool() const { return data_; }

   private:
    ScopeOwnedBySomeoneElse<stream_data_t> data_;
  };
  using publisher_t = ss::StreamPublisher<StreamPublisher, entry_t>;

  StreamImpl()
      : own_data_(),
        schema_as_object_(StaticConstructSchemaAsObject(schema_exposed_namespace_name_, schema_top_level_name_)),
        publisher_(std::make_unique<publisher_t>(own_data_)),
        authority_(StreamDataAuthority::Own) {}

  StreamImpl(const SherlockNamespaceName& exposed_namespace)
      : own_data_(),
        schema_exposed_namespace_name_(exposed_namespace.namespace_name),
        schema_top_level_name_(exposed_namespace.top_level_name),
        schema_as_object_(StaticConstructSchemaAsObject(schema_exposed_namespace_name_, schema_top_level_name_)),
        publisher_(std::make_unique<publisher_t>(own_data_)),
        authority_(StreamDataAuthority::Own) {}

  template <typename X, typename... XS, class = std::enable_if_t<!std::is_same<X, SherlockNamespaceName>::value>>
  StreamImpl(X&& x, XS&&... xs)
      : own_data_(std::forward<X>(x), std::forward<XS>(xs)...),
        schema_as_object_(StaticConstructSchemaAsObject(schema_exposed_namespace_name_, schema_top_level_name_)),
        publisher_(std::make_unique<publisher_t>(own_data_)),
        authority_(StreamDataAuthority::Own) {}

  template <typename X, typename... XS>
  StreamImpl(const SherlockNamespaceName& exposed_namespace, X&& x, XS&&... xs)
      : own_data_(std::forward<X>(x), std::forward<XS>(xs)...),
        schema_exposed_namespace_name_(exposed_namespace.namespace_name),
        schema_top_level_name_(exposed_namespace.top_level_name),
        schema_as_object_(StaticConstructSchemaAsObject(schema_exposed_namespace_name_, schema_top_level_name_)),
        publisher_(std::make_unique<publisher_t>(own_data_)),
        authority_(StreamDataAuthority::Own) {}

  StreamImpl(StreamImpl&& rhs)
      : own_data_(std::move(rhs.own_data_)),
        schema_exposed_namespace_name_(rhs.schema_exposed_namespace_name_),
        schema_top_level_name_(rhs.schema_top_level_name_),
        schema_as_object_(StaticConstructSchemaAsObject(schema_exposed_namespace_name_, schema_top_level_name_)),
        publisher_(std::move(rhs.publisher_)),
        authority_(rhs.authority_) {
    rhs.authority_ = StreamDataAuthority::External;
  }

  ~StreamImpl() {
    // TODO(dkorolev): These should be erased in an ongoing fashion.
    // Order of destruction in `http_subscriptions` does matter - scopes should be deleted first -- M.Z.
    for (auto& it : own_data_.ObjectAccessorDespitePossiblyDestructing().http_subscriptions) {
      it.second.first = nullptr;
    }
    own_data_.ObjectAccessorDespitePossiblyDestructing().http_subscriptions.clear();
  }

  void operator=(StreamImpl&& rhs) {
    own_data_ = std::move(rhs.own_data_);
    publisher_ = std::move(rhs.publisher_);
    authority_ = rhs.authority_;
    rhs.authority_ = StreamDataAuthority::External;
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

  void UpdateHead(const std::chrono::microseconds us = current::time::Now()) {
    std::lock_guard<std::mutex> lock(publisher_mutex_);
    if (publisher_) {
      return publisher_->template UpdateHead<current::locks::MutexLockStatus::AlreadyLocked>(us);
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
    bool this_is_valid_;
    std::function<void()> done_callback_;
    current::WaitableTerminateSignal terminate_signal_;
    ScopeOwnedBySomeoneElse<stream_data_t> data_;
    F& subscriber_;
    const uint64_t begin_idx_;
    std::thread thread_;

    SubscriberThreadInstance() = delete;
    SubscriberThreadInstance(const SubscriberThreadInstance&) = delete;
    SubscriberThreadInstance(SubscriberThreadInstance&&) = delete;
    void operator=(const SubscriberThreadInstance&) = delete;
    void operator=(SubscriberThreadInstance&&) = delete;

   public:
    SubscriberThreadInstance(ScopeOwned<stream_data_t>& data,
                             F& subscriber,
                             uint64_t begin_idx,
                             std::function<void()> done_callback)
        : this_is_valid_(false),
          done_callback_(done_callback),
          terminate_signal_(),
          data_(data,
                [this]() {
                  std::lock_guard<std::mutex> lock(data_.ObjectAccessorDespitePossiblyDestructing().publish_mutex);
                  terminate_signal_.SignalExternalTermination();
                }),
          subscriber_(subscriber),
          begin_idx_(begin_idx),
          thread_(&SubscriberThreadInstance::Thread, this) {
      // Must guard against the constructor of `ScopeOwnedBySomeoneElse<stream_data_t> data_` throwing.
      this_is_valid_ = true;
    }

    ~SubscriberThreadInstance() {
      if (this_is_valid_) {
        // The constructor has completed successfully. The thread has started, and `data_` is valid.
        CURRENT_ASSERT(thread_.joinable());
        if (!subscriber_thread_done_) {
          std::lock_guard<std::mutex> lock(data_.ObjectAccessorDespitePossiblyDestructing().publish_mutex);
          terminate_signal_.SignalExternalTermination();
        }
        thread_.join();
      } else {
        // The constructor has not completed successfully. The thread was not started, and `data_` is garbage.
        if (done_callback_) {
          // TODO(dkorolev): Fix this ownership issue.
          done_callback_();
        }
      }
    }

    void Thread() {
      // Keep the subscriber thread exception-safe. By construction, it's guaranteed to live
      // strictly within the scope of existence of `stream_data_t` contained in `data_`.
      stream_data_t& bare_data = data_.ObjectAccessorDespitePossiblyDestructing();
      ThreadImpl(bare_data, begin_idx_);
      subscriber_thread_done_ = true;
      std::lock_guard<std::mutex> lock(bare_data.http_subscriptions_mutex);
      if (done_callback_) {
        done_callback_();
      }
    }

    void ThreadImpl(stream_data_t& bare_data, uint64_t begin_idx) {
      auto head = std::chrono::microseconds(0);
      uint64_t index = begin_idx;
      uint64_t size = 0;
      bool terminate_sent = false;
      while (true) {
        // TODO(dkorolev): This `EXCL` section can and should be tested by subscribing to an empty stream.
        // TODO(dkorolev): This is actually more a case of `EndReached()` first, right?
        if (!terminate_sent && terminate_signal_) {
          terminate_sent = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        auto current_head = bare_data.persistence.CurrentHead();
        size = bare_data.persistence.Size();
        if (current_head > head) {
          if (size > index) {
            for (const auto& e : bare_data.persistence.Iterate(index, size)) {
              if (!terminate_sent && terminate_signal_) {
                terminate_sent = true;
                if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
                  return;
                }
              }
              head = e.idx_ts.us;
              if (current::ss::PassEntryToSubscriberIfTypeMatches<TYPE_SUBSCRIBED_TO, entry_t>(
                      subscriber_,
                      [this]() -> ss::EntryResponse { return subscriber_.EntryResponseIfNoMorePassTypeFilter(); },
                      e.entry,
                      e.idx_ts,
                      bare_data.persistence.LastPublishedIndexAndTimestamp()) == ss::EntryResponse::Done) {
                return;
              }
            }
            index = size;
          }
          if (size > begin_idx && current_head > head && subscriber_(current_head) == ss::EntryResponse::Done) {
            return;
          }
          head = current_head;
        } else {
          std::unique_lock<std::mutex> lock(bare_data.publish_mutex);
          current::WaitableTerminateSignalBulkNotifier::Scope scope(bare_data.notifier, terminate_signal_);
          terminate_signal_.WaitUntil(lock,
                                      [this, &bare_data, &index, &head]() {
                                        return terminate_signal_ || bare_data.persistence.Size() > index ||
                                               bare_data.persistence.CurrentHead() > head;
                                      });
        }
      }
    }
  };

  // Expose the means to control the scope of the subscriber.
  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t>
  class SubscriberScope final : public current::sherlock::SubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using base_t = current::sherlock::SubscriberScope;

   public:
    using subscriber_thread_t = SubscriberThreadInstance<TYPE_SUBSCRIBED_TO, F>;

    SubscriberScope(ScopeOwned<stream_data_t>& data,
                    F& subscriber,
                    uint64_t begin_idx,
                    std::function<void()> done_callback)
        : base_t(std::move(std::make_unique<subscriber_thread_t>(data, subscriber, begin_idx, done_callback))) {}
  };

  template <typename TYPE_SUBSCRIBED_TO = entry_t, typename F>
  SubscriberScope<F, TYPE_SUBSCRIBED_TO> Subscribe(F& subscriber,
                                                   uint64_t begin_idx = 0u,
                                                   std::function<void()> done_callback = nullptr) {
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    try {
      return SubscriberScope<F, TYPE_SUBSCRIBED_TO>(own_data_, subscriber, begin_idx, done_callback);
    } catch (const current::sync::InDestructingModeException&) {
      CURRENT_THROW(StreamInGracefulShutdownException());
    }
  }

  // Sherlock handler for serving stream data via HTTP (see `pubsub.h` for details).
  template <class J>
  void ServeDataViaHTTP(Request r) {
    try {
      // Prevent `own_data_` from being destroyed between the entry into this function
      // and the call to the construction of `PubSubHTTPEndpoint`.
      //
      // Granted, an overkill, as whoever could call `ServeDataViaHTTP` from another thread should have
      // its own `ScopeOwnedBySomeoneElse<>` copy of the stream object. But err on the safe side. -- D.K.
      ScopeOwnedBySomeoneElse<stream_data_t> scoped_data(own_data_, []() {});
      stream_data_t& data = *scoped_data;

      const auto request_params = ParsePubSubHTTPRequest(r);

      if (request_params.terminate_requested) {
        typename stream_data_t::http_subscriptions_t::iterator it;
        {
          // NOTE: This is not thread-safe!!!
          // The iterator may get invalidated in between the next few lines.
          // However, during the call to `= nullptr` the mutex will be locked again from within the
          // thread terminating callback. Need to clean this up. TODO(dkorolev).
          std::lock_guard<std::mutex> lock(data.http_subscriptions_mutex);
          it = data.http_subscriptions.find(request_params.terminate_id);
        }
        if (it != data.http_subscriptions.end()) {
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

      // Unsupported HTTP method.
      if (r.method != "GET" && r.method != "HEAD") {
        r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
        return;
      }

      const auto stream_size = data.persistence.Size();

      if (request_params.size_only) {
        // Return the number of entries in the stream in `X-Current-Stream-Size` header and in the body in
        // case of `GET` method.
        const std::string size_str = current::ToString(stream_size);
        const std::string body = (r.method == "GET") ? size_str + '\n' : "";
        r(body,
          HTTPResponseCode.OK,
          current::net::constants::kDefaultContentType,
          current::net::http::Headers({{kSherlockHeaderCurrentStreamSize, size_str}}));
        return;
      }

      if (request_params.schema_requested) {
        const std::string& schema_format = request_params.schema_format;
        // Return the schema the user is requesting, in a top-level, or more fine-grained format.
        if (schema_format.empty()) {
          r(schema_as_http_response_);
        } else if (schema_format == "simple") {
          r(SubscribableSherlockSchema(
              schema_as_object_.type_id, schema_top_level_name_, schema_exposed_namespace_name_));
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
      } else {
        uint64_t begin_idx = 0u;
        std::chrono::microseconds from_timestamp(0);
        if (request_params.tail > 0u) {
          const uint64_t idx_by_tail = request_params.tail < stream_size ? (stream_size - request_params.tail) : 0u;
          begin_idx = std::max(request_params.i, idx_by_tail);
        } else if (request_params.recent.count() > 0) {
          from_timestamp = r.timestamp - request_params.recent;
        } else if (request_params.since.count() > 0) {
          from_timestamp = request_params.since;
        } else {
          begin_idx = request_params.i;
        }

        if (from_timestamp.count() > 0) {
          const auto idx_by_timestamp =
              std::min(data.persistence.IndexRangeByTimestampRange(from_timestamp, std::chrono::microseconds(0)).first,
                       stream_size);
          begin_idx = std::max(begin_idx, idx_by_timestamp);
        }

        if (request_params.no_wait && begin_idx >= stream_size) {
          // Return "200 OK" if there is nothing to return now and we were asked to not wait for new entries.
          r("", HTTPResponseCode.OK);
          return;
        }

        const std::string subscription_id = data.GenerateRandomHTTPSubscriptionID();

        auto http_chunked_subscriber = std::make_unique<PubSubHTTPEndpoint<entry_t, PERSISTENCE_LAYER, J>>(
            subscription_id, scoped_data, std::move(r), std::move(request_params));

        current::sherlock::SubscriberScope http_chunked_subscriber_scope =
            Subscribe(*http_chunked_subscriber,
                      begin_idx,
                      [this, &data, subscription_id]() {
                        // NOTE: Need to figure out when and where to lock.
                        // Chat w/ Max about the logic to clean up completed listeners.
                        // std::lock_guard<std::mutex> lock(inner_data.http_subscriptions_mutex);
                        data.http_subscriptions[subscription_id].second = nullptr;
                      });

        {
          std::lock_guard<std::mutex> lock(data.http_subscriptions_mutex);
          // TODO(dkorolev): This condition is to be rewritten correctly.
          if (!data.http_subscriptions.count(subscription_id)) {
            data.http_subscriptions[subscription_id] =
                std::make_pair(std::move(http_chunked_subscriber_scope), std::move(http_chunked_subscriber));
          }
        }
      }
    } catch (const current::sync::InDestructingModeException&) {
      r("", HTTPResponseCode.ServiceUnavailable);
    }
  }

  void operator()(Request r) {
    if (r.url.query.has("json")) {
      const auto& json = r.url.query["json"];
      if (json == "js") {
        ServeDataViaHTTP<JSONFormat::Minimalistic>(std::move(r));
      } else if (json == "fs") {
        ServeDataViaHTTP<JSONFormat::NewtonsoftFSharp>(std::move(r));
      } else {
        r("The `?json` parameter is invalid, legal values are `js`, `fs`, or omit the parameter.\n",
          HTTPResponseCode.NotFound);
      }
    } else {
      ServeDataViaHTTP<JSONFormat::Current>(std::move(r));
    }
  }

  persistence_layer_t& InternalExposePersister() {
    return own_data_.ObjectAccessorDespitePossiblyDestructing().persistence;
  }

 private:
  ScopeOwnedByMe<stream_data_t> own_data_;

  struct FillPerLanguageSchema {
    SherlockSchema& schema_ref;
    const std::string& exposed_namespace_name;
    const std::string& top_level_name;
    explicit FillPerLanguageSchema(SherlockSchema& schema,
                                   const std::string& exposed_namespace_name,
                                   const std::string& top_level_name)
        : schema_ref(schema), exposed_namespace_name(exposed_namespace_name), top_level_name(top_level_name) {}
    template <current::reflection::Language language>
    void PerLanguage() {
      schema_ref.language[current::ToString(language)] = schema_ref.type_schema.Describe<language>(
          current::reflection::NamespaceToExpose(exposed_namespace_name).template AddType<entry_t>(top_level_name));
    }
  };

  static SherlockSchema StaticConstructSchemaAsObject(const std::string& exposed_namespace_name,
                                                      const std::string& top_level_name) {
    SherlockSchema schema;

    schema.type_name = current::reflection::CurrentTypeName<entry_t>();
    schema.type_id =
        Value<current::reflection::ReflectedTypeBase>(current::reflection::Reflector().ReflectType<entry_t>()).type_id;

    reflection::StructSchema underlying_type_schema;
    underlying_type_schema.AddType<entry_t>();
    schema.type_schema = underlying_type_schema.GetSchemaInfo();

    current::reflection::ForEachLanguage(FillPerLanguageSchema(schema, exposed_namespace_name, top_level_name));

    return schema;
  }

 private:
  const std::string schema_exposed_namespace_name_ = constants::kDefaultNamespaceName;
  const std::string schema_top_level_name_ = constants::kDefaultTopLevelName;
  const SherlockSchema schema_as_object_;
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
