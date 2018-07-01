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

#ifndef CURRENT_STREAM_STREAM_H
#define CURRENT_STREAM_STREAM_H

#include "../port.h"

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "exceptions.h"
#include "stream_impl.h"
#include "pubsub.h"

#include "../typesystem/struct.h"
#include "../typesystem/schema/schema.h"

#include "../blocks/http/api.h"
#include "../blocks/persistence/memory.h"
#include "../blocks/persistence/file.h"
#include "../blocks/ss/ss.h"
#include "../blocks/ss/signature.h"

#include "../bricks/sync/locks.h"
#include "../bricks/sync/owned_borrowed.h"
#include "../bricks/time/chrono.h"
#include "../bricks/util/sha256.h"
#include "../bricks/util/waitable_terminate_signal.h"

// Stream is the overlord of streamed data storage and processing in Current.
// Stream's streams are persistent, immutable, append-only typed sequences of records ("entries").
// Each record is annotated with its 0-based index and its epoch microsecond timestamp.
// Within the stream, timestamps are strictly increasing.
//
// An in-memory stream can be constructed as `auto my_stream = stream::Stream<ENTRY>::CreateStream();`.
//
// To create a persisted one, pass in the type of persister and its construction parameters, such as:
// `auto my_stream = stream::Stream<ENTRY, current::persistence::File>::CreateStream("data.json");`.
//
// Stream streams can be published into and subscribed to.
//
// Publishing is done via `my_stream.Publisher()->Publish(ENTRY{...});`.
// At a given point in time, there can only be one master ("owned") publisher of the stream, as well as
// any number of "borrowed" publishers, forked off the master one.
//
// Subscription is done via `auto scope = my_stream.Subscribe(my_subscriber);`, where `my_subscriber`
// is an instance of the class doing the subscription. Stream runs each subscriber in a dedicated thread.
//
// Stack ownership of `my_subscriber` is respected, and `SubscriberScope` is returned for the user to store.
// As the returned `scope` object leaves the scope, the subscriber is sent a signal to terminate,
// and the destructor of `scope` waits for the subscriber to do so. The `scope` objects can be `std::move()`-d.
//
// The `my_subscriber` object should be an instance of `StreamSubscriber<IMPL, ENTRY>`,

namespace current {
namespace stream {

namespace constants {

constexpr const char* kDefaultNamespaceName = "StreamSchema";
constexpr const char* kDefaultTopLevelName = "TopLevelTransaction";

}  // namespace constants

CURRENT_STRUCT(StreamSchema) {
  CURRENT_FIELD(language, (std::map<std::string, std::string>));
  CURRENT_FIELD(type_name, std::string);
  CURRENT_FIELD(type_id, current::reflection::TypeID, current::reflection::TypeID::UninitializedType);
  CURRENT_FIELD(type_schema, reflection::SchemaInfo);
};

CURRENT_STRUCT(SubscribableStreamSchema) {
  CURRENT_FIELD(type_id, current::reflection::TypeID, current::reflection::TypeID::UninitializedType);
  CURRENT_FIELD(entry_name, std::string);
  CURRENT_FIELD(namespace_name, std::string);

  CURRENT_DEFAULT_CONSTRUCTOR(SubscribableStreamSchema) {}
  CURRENT_CONSTRUCTOR(SubscribableStreamSchema)(
      current::reflection::TypeID type_id, const std::string& entry_name, const std::string& namespace_name)
      : type_id(type_id), entry_name(entry_name), namespace_name(namespace_name) {}

  bool operator==(const SubscribableStreamSchema& rhs) const {
    return type_id == rhs.type_id && namespace_name == rhs.namespace_name && entry_name == rhs.entry_name;
  }
  bool operator!=(const SubscribableStreamSchema& rhs) const { return !operator==(rhs); }
};

CURRENT_STRUCT(StreamSchemaFormatNotFoundError) {
  CURRENT_FIELD(error, std::string, "Unsupported schema format requested.");
  CURRENT_FIELD(unsupported_format_requested, Optional<std::string>);
};

template <typename ENTRY>
using DEFAULT_PERSISTENCE_LAYER = current::persistence::Memory<ENTRY>;

enum class SubscriptionMode : bool { Checked = true, Unchecked = false };

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER = DEFAULT_PERSISTENCE_LAYER>
class Stream final {
 public:
  // Inner types.
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<entry_t>;
  using impl_t = StreamImpl<entry_t, PERSISTENCE_LAYER>;
  using publisher_t = ss::StreamPublisher<StreamPublisherImpl<ENTRY, PERSISTENCE_LAYER>, entry_t>;

 private:
  // Data members.
  // The `const` ones to be initialized first (just in case they are needed while `Stream` is being destructed).
  const ss::StreamNamespaceName schema_namespace_name_ =
      ss::StreamNamespaceName(constants::kDefaultNamespaceName, constants::kDefaultTopLevelName);

  const StreamSchema schema_as_object_;

  const Response schema_as_http_response_ = Response(JSON<JSONFormat::Minimalistic>(schema_as_object_),
                                                     HTTPResponseCode.OK,
                                                     current::net::constants::kDefaultJSONContentType);

  // The unique instance of stream data and associated logic (mutexes, HTTP subscribers list, etc.)
  Owned<impl_t> impl_;

  // The unique instance of the publisher. Can be recreated to notify all presently borrowed publishers
  // and wait until all of them are released.
  Optional<Owned<publisher_t>> owned_publisher_;

  // The "reference" to the publisher that can be borrowed -- iff the stream owns its publisher, i.e. is the Master.
  // DIMA Optional<BorrowedOfGuaranteedLifetime<publisher_t>> borrowable_publisher_;
  Optional<Borrowed<publisher_t>> borrowable_publisher_;

 public:
  // "Constructors" and the destructor.
  template <typename... ARGS>
  static Owned<Stream> CreateStream(ARGS&&... args) {
    return current::MakeOwned<Stream>(PrivateConstructorWithoutNamespace(), std::forward<ARGS>(args)...);
  }

  template <typename... ARGS>
  static Owned<Stream> CreateStreamWithCustomNamespaceName(const ss::StreamNamespaceName& namespace_name,
                                                           ARGS&&... args) {
    return current::MakeOwned<Stream>(PrivateConstructorWithNamespace(), namespace_name, std::forward<ARGS>(args)...);
  }

  ~Stream() {
    // Order of destruction in `http_subscriptions` does matter - scopes should be deleted first -- M.Z.
    for (auto& it : impl_->http_subscriptions) {
      it.second.first = nullptr;
    }
    impl_->http_subscriptions.clear();
  }

 private:
  // Magic to enable `current::MakeOwned<Stream>` create instances of `Stream`.
  struct PrivateConstructorWithoutNamespace {};
  struct PrivateConstructorWithNamespace {};
  friend struct sync::impl::UniqueInstance<Stream>;

  template <typename... ARGS>
  Stream(PrivateConstructorWithoutNamespace, ARGS&&... args)
      : schema_as_object_(StaticConstructSchemaAsObject(schema_namespace_name_)),
        impl_(MakeOwned<impl_t>(schema_namespace_name_, std::forward<ARGS>(args)...)),
        owned_publisher_(MakeOwned<publisher_t>(impl_)),
        borrowable_publisher_(Value(owned_publisher_)) {}

  template <typename... ARGS>
  Stream(PrivateConstructorWithNamespace, const ss::StreamNamespaceName& namespace_name, ARGS&&... args)
      : schema_namespace_name_(namespace_name),
        schema_as_object_(StaticConstructSchemaAsObject(schema_namespace_name_)),
        impl_(MakeOwned<impl_t>(schema_namespace_name_, std::forward<ARGS>(args)...)),
        owned_publisher_(MakeOwned<publisher_t>(impl_)),
        borrowable_publisher_(Value(owned_publisher_)) {}

  // `RecreatePublisher` invalidates all external publishers to this stream and creates a fresh new publisher,
  // which then can be stored in the stream (Master mode) or given out of it (Following mode).
  // NOTE: The call to `RecreatePublisher` will wait indefinitely if external publishers are not giving up.
  Borrowed<publisher_t> RecreatePublisher() {
    // Kill existing borrowers of the publisher. Wait as needed, for as long as needed.
    // This is essential to gracefully invalidate all the previously-issued borrowed publishers.
    borrowable_publisher_ = nullptr;
    owned_publisher_ = nullptr;
    // Create a fresh new publisher.
    owned_publisher_ = MakeOwned<publisher_t>(impl_);
    return Value(owned_publisher_);
  }

 public:
  // `Publisher()`: The caller assumes full responsibility for making sure the underlying stream
  // is not destroyed while it is being published to.
  // CAN THROW `PublisherNotAvailableException`.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  WeakBorrowed<publisher_t>& Publisher() {
    current::locks::SmartMutexLockGuard<MLS> lock(impl_->publishing_mutex);
    if (Exists(borrowable_publisher_)) {
      return Value(borrowable_publisher_);
    } else {
      CURRENT_THROW(PublisherNotAvailableException());
    }
  }

  // `BorrowPublisher()`: The caller can count on the fact that if the call succeeded,
  // the returned borrowed publisher will be valid until voluntarily released.
  // CAN THROW `PublisherNotAvailableException`.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  Borrowed<publisher_t> BorrowPublisher() {
    current::locks::SmartMutexLockGuard<MLS> lock(impl_->publishing_mutex);
    if (Exists(borrowable_publisher_)) {
      return Value(borrowable_publisher_);
    } else {
      CURRENT_THROW(PublisherNotAvailableException());
    }
  }

  // Master-follower logic.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  bool IsMasterStream() const {
    current::locks::SmartMutexLockGuard<MLS> lock(impl_->publishing_mutex);
    return Exists(borrowable_publisher_);
  }

  // `BecomeMasterStream` invalidates all external publishers to this stream, and restores "the order",
  // where to publish to the stream a publisher should be retrieved directly from the stream.
  // NOTE: The call to `BecomeMasterStream` will wait indefinitely if external publishers are not giving up.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void BecomeMasterStream() {
    current::locks::SmartMutexLockGuard<MLS> lock(impl_->publishing_mutex);
    borrowable_publisher_ = RecreatePublisher();
  }

  // `BecomeFollowingStream` invalidates all external publishers to this stream,
  // but instead of "restoring the order" where this stream can publish into itself,
  // it gives its publisher away, so the only possible way to publish into this stream
  // is to borrow the publisher from the caller of `BecomeFollowingStream`, not from the stream itself.
  // NOTE: The call to `BecomeFollowingStream` will wait indefinitely if external publishers are not giving up.
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  Borrowed<publisher_t> BecomeFollowingStream() {
    current::locks::SmartMutexLockGuard<MLS> lock(impl_->publishing_mutex);
    return RecreatePublisher();
  }

  // TODO(dkorolev): Master-follower flip between two streams belongs in Stream first, then in Storage.
  template <typename TYPE_SUBSCRIBED_TO, typename F, SubscriptionMode SM>
  class SubscriberThreadInstance final : public current::stream::SubscriberScope::SubscriberThread {
   private:
    bool this_is_valid_;
    std::function<void()> done_callback_;
    current::WaitableTerminateSignal terminate_signal_;
    bool terminate_sent_;
    BorrowedWithCallback<impl_t> impl_;
    F& subscriber_;
    const uint64_t begin_idx_;
    const std::chrono::microseconds from_us_;
    std::thread thread_;

    SubscriberThreadInstance() = delete;
    SubscriberThreadInstance(const SubscriberThreadInstance&) = delete;
    SubscriberThreadInstance(SubscriberThreadInstance&&) = delete;
    void operator=(const SubscriberThreadInstance&) = delete;
    void operator=(SubscriberThreadInstance&&) = delete;

   public:
    SubscriberThreadInstance(Borrowed<impl_t> impl,
                             F& subscriber,
                             uint64_t begin_idx,
                             std::chrono::microseconds from_us,
                             std::function<void()> done_callback)
        : this_is_valid_(false),
          done_callback_(done_callback),
          terminate_signal_(),
          terminate_sent_(false),
          impl_(std::move(impl),
                [this]() {
                  // NOTE(dkorolev): I'm uncertain whether this lock is necessary here. Keeping it for safety now.
                  std::lock_guard<std::mutex> lock(impl_->publishing_mutex);
                  terminate_signal_.SignalExternalTermination();
                }),
          subscriber_(subscriber),
          begin_idx_(begin_idx),
          from_us_(from_us),
          thread_(&SubscriberThreadInstance::Thread, this) {
      // Must guard against the constructor of `BorrowedWithCallback<impl_t> impl_` throwing.
      // NOTE(dkorolev): This is obsolete now, but keeping the logic for now, to keep it safe. -- D.K.
      this_is_valid_ = true;
    }

    ~SubscriberThreadInstance() {
      if (this_is_valid_) {
        // The constructor has completed successfully. The thread has started, and `impl_` is valid.
        CURRENT_ASSERT(thread_.joinable());
        if (!subscriber_thread_done_) {
          std::lock_guard<std::mutex> lock(impl_->publishing_mutex);
          terminate_signal_.SignalExternalTermination();
        }
        thread_.join();
      } else {
        // The constructor has not completed successfully. The thread was not started, and `impl_` is garbage.
        if (done_callback_) {
          done_callback_();
        }
      }
    }

    void Thread() {
      // Keep the subscriber thread exception-safe. By construction, it's guaranteed to live
      // strictly within the scope of existence of `impl_t` contained in `impl_`.
      ThreadImpl(begin_idx_);
      subscriber_thread_done_ = true;
      std::lock_guard<std::mutex> lock(impl_->http_subscriptions_mutex);
      if (done_callback_) {
        done_callback_();
      }
    }

    template <SubscriptionMode MODE = SM>
    ENABLE_IF<MODE == SubscriptionMode::Checked, ss::EntryResponse> PassEntriesToSubscriber(const impl_t& impl,
                                                                                            uint64_t index,
                                                                                            uint64_t size) {
      for (const auto& e : impl.persister.Iterate(index, size)) {
        if (!terminate_sent_ && terminate_signal_) {
          terminate_sent_ = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return ss::EntryResponse::Done;
          }
        }
        if (current::ss::PassEntryToSubscriberIfTypeMatches<TYPE_SUBSCRIBED_TO, entry_t>(
                subscriber_,
                [this]() -> ss::EntryResponse { return subscriber_.EntryResponseIfNoMorePassTypeFilter(); },
                e.entry,
                e.idx_ts,
                impl.persister.LastPublishedIndexAndTimestamp()) == ss::EntryResponse::Done) {
          return ss::EntryResponse::Done;
        }
      }
      return ss::EntryResponse::More;
    }

    template <SubscriptionMode MODE = SM>
    ENABLE_IF<MODE == SubscriptionMode::Unchecked, ss::EntryResponse> PassEntriesToSubscriber(const impl_t& impl,
                                                                                              uint64_t index,
                                                                                              uint64_t size) {
      for (const auto& e : impl.persister.IterateUnsafe(index, size)) {
        if (!terminate_sent_ && terminate_signal_) {
          terminate_sent_ = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return ss::EntryResponse::Done;
          }
        }
        if (subscriber_(e, index++, impl.persister.LastPublishedIndexAndTimestamp()) == ss::EntryResponse::Done) {
          return ss::EntryResponse::Done;
        }
      }
      return ss::EntryResponse::More;
    }

    void ThreadImpl(uint64_t begin_idx) {
      auto head = from_us_ - std::chrono::microseconds(1);
      uint64_t index = begin_idx;
      uint64_t size = 0;
      while (true) {
        if (!terminate_sent_ && terminate_signal_) {
          terminate_sent_ = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        const auto head_idx = impl_->persister.HeadAndLastPublishedIndexAndTimestamp();
        size = Exists(head_idx.idxts) ? Value(head_idx.idxts).index + 1 : 0;
        if (head_idx.head > head) {
          if (size > index) {
            if (PassEntriesToSubscriber(*impl_, index, size) == ss::EntryResponse::Done) {
              return;
            }
            index = size;
            head = Value(head_idx.idxts).us;
          }
          if (size >= begin_idx && head_idx.head > head && subscriber_(head_idx.head) == ss::EntryResponse::Done) {
            return;
          }
          head = head_idx.head;
        } else {
          std::unique_lock<std::mutex> lock(impl_->publishing_mutex);
          current::WaitableTerminateSignalBulkNotifier::Scope scope(impl_->notifier, terminate_signal_);
          terminate_signal_.WaitUntil(
              lock,
              [this, &index, &begin_idx, &head]() {
                return terminate_signal_ ||
                       impl_->persister.template Size<current::locks::MutexLockStatus::AlreadyLocked>() > index ||
                       (index > begin_idx &&
                        impl_->persister.template CurrentHead<current::locks::MutexLockStatus::AlreadyLocked>() > head);
              });
        }
      }
    }
  };

  // Expose the means to control the scope of the subscriber.
  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t, SubscriptionMode SM = SubscriptionMode::Unchecked>
  class SubscriberScopeImpl final : public current::stream::SubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using base_t = current::stream::SubscriberScope;

   public:
    using subscriber_thread_t = SubscriberThreadInstance<TYPE_SUBSCRIBED_TO, F, SM>;

    SubscriberScopeImpl(Borrowed<impl_t> impl,
                        F& subscriber,
                        uint64_t begin_idx,
                        std::chrono::microseconds from_us,
                        std::function<void()> done_callback)
        : base_t(std::move(
              std::make_unique<subscriber_thread_t>(std::move(impl), subscriber, begin_idx, from_us, done_callback))) {}

    SubscriberScopeImpl(SubscriberScopeImpl&&) = default;
    SubscriberScopeImpl& operator=(SubscriberScopeImpl&&) = default;

    SubscriberScopeImpl() = delete;
    SubscriberScopeImpl(const SubscriberScopeImpl&) = delete;
    SubscriberScopeImpl& operator=(const SubscriberScopeImpl&) = delete;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO = entry_t>
  using SubscriberScope = SubscriberScopeImpl<F, TYPE_SUBSCRIBED_TO, SubscriptionMode::Checked>;
  template <typename F>
  using SubscriberScopeUnchecked = SubscriberScopeImpl<F>;

  template <typename TYPE_SUBSCRIBED_TO = entry_t, typename F>
  SubscriberScope<F, TYPE_SUBSCRIBED_TO> Subscribe(F& subscriber,
                                                   uint64_t begin_idx = 0u,
                                                   std::chrono::microseconds from_us = std::chrono::microseconds(0),
                                                   std::function<void()> done_callback = nullptr) const {
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    return SubscriberScope<F, TYPE_SUBSCRIBED_TO>(impl_, subscriber, begin_idx, from_us, done_callback);
  }

  template <typename F>
  SubscriberScopeUnchecked<F> SubscribeUnchecked(F& subscriber,
                                                 uint64_t begin_idx = 0u,
                                                 std::chrono::microseconds from_us = std::chrono::microseconds(0),
                                                 std::function<void()> done_callback = nullptr) const {
    return SubscriberScopeUnchecked<F>(impl_, subscriber, begin_idx, from_us, done_callback);
  }

  // Generates a random HTTP subscription.
  static std::string GenerateRandomHTTPSubscriptionID() {
    return current::SHA256("stream_http_subscription_" +
                           current::ToString(current::random::CSRandomUInt64(0ull, ~0ull)));
  }

  // Stream handler for serving stream data via HTTP (see `pubsub.h` for details).
  template <class J>
  void ServeDataViaHTTP(Request r) const {
    if (r.method != "GET" && r.method != "HEAD") {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
      return;
    }

    // First and foremost, save ourselves the trouble of starting the subscriber
    // if the stream is already being destructed.
    const Borrowed<impl_t> borrowed_impl(impl_);
    if (!borrowed_impl) {
      r("", HTTPResponseCode.ServiceUnavailable);
      return;
    }

    auto request_params = ParsePubSubHTTPRequest(r);  // Mutable as `tail` may change. -- D.K.

    if (request_params.terminate_requested) {
      typename impl_t::http_subscriptions_t::iterator it;
      {
        // NOTE: This is not thread-safe!!!
        // The iterator may get invalidated in between the next few lines.
        // However, during the call to `= nullptr` the mutex will be locked again from within the
        // thread terminating callback. Need to clean this up. TODO(dkorolev).
        std::lock_guard<std::mutex> lock(borrowed_impl->http_subscriptions_mutex);
        it = borrowed_impl->http_subscriptions.find(request_params.terminate_id);
      }
      if (it != borrowed_impl->http_subscriptions.end()) {
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

    const auto stream_size = borrowed_impl->persister.Size();

    if (request_params.size_only) {
      // Return the number of entries in the stream in `X-Current-Stream-Size` header
      // and in the body in case of the `GET` method.
      const std::string size_str = current::ToString(stream_size);
      const std::string body = (r.method == "GET") ? size_str + '\n' : "";
      r(body,
        HTTPResponseCode.OK,
        current::net::http::Headers({{kStreamHeaderCurrentStreamSize, size_str}}),
        current::net::constants::kDefaultContentType);
      return;
    }

    if (request_params.schema_requested) {
      const std::string& schema_format = request_params.schema_format;
      // Return the schema the user is requesting, in a top-level, or more fine-grained format.
      if (schema_format.empty()) {
        r(schema_as_http_response_);
      } else if (schema_format == "simple") {
        r(SubscribableStreamSchema(
            schema_as_object_.type_id, schema_namespace_name_.entry_name, schema_namespace_name_.namespace_name));
      } else {
        const auto cit = schema_as_object_.language.find(schema_format);
        if (cit != schema_as_object_.language.end()) {
          r(cit->second);
        } else {
          StreamSchemaFormatNotFoundError four_oh_four;
          four_oh_four.unsupported_format_requested = schema_format;
          r(four_oh_four, HTTPResponseCode.NotFound);
        }
      }
    } else {
      uint64_t begin_idx = 0u;
      std::chrono::microseconds from_timestamp(0);
      if (request_params.tail) {
        if (request_params.tail == static_cast<uint64_t>(-1)) {
          // Special case for `&tail=0`: act as `tail -f` from the current end of the stream.
          begin_idx = stream_size;
          request_params.tail = stream_size;
        } else {
          const uint64_t idx_by_tail = request_params.tail < stream_size ? (stream_size - request_params.tail) : 0u;
          begin_idx = std::max(request_params.i, idx_by_tail);
        }
      } else if (request_params.recent.count() > 0) {
        from_timestamp = r.timestamp - request_params.recent;
      } else if (request_params.since.count() > 0) {
        from_timestamp = request_params.since;
      } else {
        begin_idx = request_params.i;
      }

      if (from_timestamp.count() > 0) {
        const auto idx_by_timestamp =
            std::min(borrowed_impl->persister.IndexRangeByTimestampRange(from_timestamp).first, stream_size);
        begin_idx = std::max(begin_idx, idx_by_timestamp);
      }

      if (request_params.no_wait && begin_idx >= stream_size) {
        // Return "204 No Content" if there is nothing to return now and we were asked to not wait for new entries.
        r("", HTTPResponseCode.NoContent);
        return;
      }

      const std::string subscription_id = GenerateRandomHTTPSubscriptionID();

      auto http_chunked_subscriber = std::make_unique<PubSubHTTPEndpoint<entry_t, PERSISTENCE_LAYER, J>>(
          subscription_id, borrowed_impl, std::move(r), std::move(request_params));
      const auto done_callback = [borrowed_impl, subscription_id]() {
        // Note: Called from a locked section of `borrowed_impl->http_subscriptions_mutex`.
        borrowed_impl->http_subscriptions[subscription_id].second = nullptr;
      };
      current::stream::SubscriberScope http_chunked_subscriber_scope =
          request_params.checked ? static_cast<current::stream::SubscriberScope>(
                                       Subscribe(*http_chunked_subscriber, begin_idx, from_timestamp, done_callback))
                                 : static_cast<current::stream::SubscriberScope>(SubscribeUnchecked(
                                       *http_chunked_subscriber, begin_idx, from_timestamp, done_callback));

      {
        std::lock_guard<std::mutex> lock(borrowed_impl->http_subscriptions_mutex);
        // Note: Silently relying on no collision here. Suboptimal.
        if (!borrowed_impl->http_subscriptions.count(subscription_id)) {
          borrowed_impl->http_subscriptions[subscription_id] =
              std::make_pair(std::move(http_chunked_subscriber_scope), std::move(http_chunked_subscriber));
        }
      }
    }
  }

  void operator()(Request r) {
    if (r.url.query.has("json")) {
      const auto& json = r.url.query["json"];
      if (json == "minimalistic") {
        ServeDataViaHTTP<JSONFormat::Minimalistic>(std::move(r));
      } else if (json == "js") {
        ServeDataViaHTTP<JSONFormat::JavaScript>(std::move(r));
      } else if (json == "fs") {
        ServeDataViaHTTP<JSONFormat::NewtonsoftFSharp>(std::move(r));
      } else {
        r("The `?json` parameter is invalid, legal values are `minimalistic`, `js`, `fs`, or omit the parameter.\n",
          HTTPResponseCode.NotFound);
      }
    } else {
      ServeDataViaHTTP<JSONFormat::Current>(std::move(r));
    }
  }

  Borrowed<impl_t> BorrowImpl() const { return impl_; }
  const WeakBorrowed<impl_t>& Impl() const { return impl_; }
  WeakBorrowed<impl_t>& Impl() { return impl_; }

  // `Entries` never throws. It enables iterating over the stream's impl in synchronous fashion
  // ("IEnumerable", as opposed to "IObservable"), although its primary usecase is `.Size()` in the unit test.
  // NOTE(dkorolev): Returns the pointer, because we're using `->` everywhere now. -- D.K.
  const persistence_layer_t* Data() const { return &impl_->persister; }

  static StreamSchema StaticConstructSchemaAsObject(const ss::StreamNamespaceName& namespace_name) {
    StreamSchema schema;

    // TODO(dkorolev): `AsIdentifier` here?
    schema.type_name = current::reflection::CurrentTypeName<entry_t, current::reflection::NameFormat::Z>();
    schema.type_id =
        Value<current::reflection::ReflectedTypeBase>(current::reflection::Reflector().ReflectType<entry_t>()).type_id;

    reflection::StructSchema underlying_type_schema;
    underlying_type_schema.AddType<entry_t>();
    schema.type_schema = underlying_type_schema.GetSchemaInfo();

    current::reflection::ForEachLanguage(FillPerLanguageSchema(schema, namespace_name));

    return schema;
  }

 private:
  struct FillPerLanguageSchema {
    StreamSchema& schema_ref;
    const ss::StreamNamespaceName& namespace_name;
    explicit FillPerLanguageSchema(StreamSchema& schema, const ss::StreamNamespaceName& namespace_name)
        : schema_ref(schema), namespace_name(namespace_name) {}
    template <current::reflection::Language language>
    void PerLanguage() {
      schema_ref.language[current::ToString(language)] = schema_ref.type_schema.Describe<language>(
          current::reflection::NamespaceToExpose(namespace_name.namespace_name)
              .template AddType<entry_t>(namespace_name.entry_name));
    }
  };

 private:
  // `Stream` instances are meant to be used as `Owned<Stream>`, created via `Stream<...>::CreateStream(...)`.
  // No need for moving those around whatsoever.
  Stream() = delete;
  Stream(const Stream&) = delete;
  Stream& operator=(const Stream&) = delete;
  Stream(Stream&& rhs) = delete;
  Stream& operator=(Stream&& rhs) = delete;
};

}  // namespace stream
}  // namespace current

#endif  // CURRENT_STREAM_STREAM_H
