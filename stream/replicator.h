/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>
          (c) 2017 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2018 Dima Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STREAM_REPLICATOR_H
#define CURRENT_STREAM_REPLICATOR_H

#include <functional>
#include <string>
#include <thread>

#include "exceptions.h"
#include "stream.h"
#include "stream_impl.h"

#include "../blocks/http/api.h"
#include "../blocks/ss/ss.h"

#include "../bricks/sync/owned_borrowed.h"
#include "../bricks/sync/waitable_atomic.h"

#include "../typesystem/reflection/types.h"

namespace current {
namespace stream {

enum class ReplicationMode : bool { Checked = true, Unchecked = false };

template <typename STREAM_ENTRY>
class SubscribableRemoteStream final {
 public:
  using entry_t = STREAM_ENTRY;

  class RemoteStream final {
   public:
    RemoteStream(const std::string& url, const std::string& entry_name, const std::string& namespace_name)
        : url_(url),
          schema_(Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<entry_t>()).type_id,
                  entry_name,
                  namespace_name) {}

    void CheckSchema() const {
      const auto response = HTTP(GET(url_ + "/schema.simple"));
      if (response.code == HTTPResponseCode.OK) {
        const auto remote_schema = ParseJSON<SubscribableStreamSchema>(response.body);
        if (remote_schema != schema_) {
          CURRENT_THROW(RemoteStreamInvalidSchemaException());
        }
      } else {
        CURRENT_THROW(RemoteStreamDoesNotRespondException());
      }
    }

    uint64_t GetNumberOfEntries() const {
      const auto response = HTTP(GET(url_ + "?sizeonly"));
      if (response.code == HTTPResponseCode.OK) {
        return FromString<uint64_t>(response.body);
      } else {
        CURRENT_THROW(RemoteStreamDoesNotRespondException());
      }
    }

    std::string GetURLToSubscribe(uint64_t index, SubscriptionMode mode) const {
      return url_ + "?i=" + current::ToString(index) + (mode == SubscriptionMode::Checked ? "&checked" : "");
    }

    std::string GetURLToTerminate(const std::string& subscription_id) const {
      return url_ + "?terminate=" + subscription_id;
    }

    std::string GetFlipToMasterURL(head_optidxts_t head_idxts, uint64_t key, SubscriptionMode mode) const {
      std::string opt;
      if (Exists(head_idxts.idxts)) {
        const auto last_idx = Value(head_idxts.idxts).index;
        opt = "i=" + current::ToString(last_idx);
      }
      opt += "&head=" + current::ToString(head_idxts.head);
      return url_ + "/become/master?" + opt + "&key=" + current::ToString(key) + "&clock=" +
             current::ToString(current::time::Now().count()) + (mode == SubscriptionMode::Checked ? "&checked" : "");
    }

   private:
    const std::string url_;
    const SubscribableStreamSchema schema_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO, ReplicationMode RM>
  class RemoteStreamSubscriber {
    static_assert(current::ss::IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");

   public:
    RemoteStreamSubscriber(Borrowed<RemoteStream> remote_stream,
                           F& subscriber,
                           uint64_t start_idx,
                           std::function<void()> destruction_callback)
        : borrowed_remote_stream_(std::move(remote_stream), destruction_callback),
          subscriber_(subscriber),
          index_(start_idx),
          unused_idxts_() {}

    void PassChunkToSubscriber(const std::string& chunk) {
      const size_t chunk_size = chunk.size();
      size_t end_pos = 0;
      if (!carried_over_data_.empty()) {
        while (end_pos < chunk_size && chunk[end_pos] != '\n' && chunk[end_pos] != '\r') {
          ++end_pos;
        }
        if (end_pos == chunk_size) {
          carried_over_data_ += chunk;
          return;
        }
        PassEntryToSubscriber(carried_over_data_ + chunk.substr(0, end_pos));
      }

      size_t start_pos = end_pos;
      for (;;) {
        while (start_pos < chunk_size && (chunk[start_pos] == '\n' || chunk[start_pos] == '\r')) {
          ++start_pos;
        }
        end_pos = start_pos + 1;
        while (end_pos < chunk_size && chunk[end_pos] != '\n' && chunk[end_pos] != '\r') {
          ++end_pos;
        }
        if (end_pos >= chunk_size) {
          break;
        }
        PassEntryToSubscriber(chunk.substr(start_pos, end_pos - start_pos));
        start_pos = end_pos + 1;
      }
      if (start_pos < chunk_size) {
        carried_over_data_ = chunk.substr(start_pos);
      } else {
        carried_over_data_.clear();
      }
    }

   protected:
    BorrowedWithCallback<RemoteStream> borrowed_remote_stream_;
    F& subscriber_;
    uint64_t index_;
    const idxts_t unused_idxts_;
    std::string carried_over_data_;

   private:
    template <ReplicationMode MODE = RM>
    ENABLE_IF<MODE == ReplicationMode::Checked> PassEntryToSubscriber(const std::string& raw_log_line) {
      const auto split = current::strings::Split(raw_log_line, '\t');
      if (split.empty()) {
        CURRENT_THROW(RemoteStreamMalformedChunkException());
      }
      const auto tsoptidx = ParseJSON<ts_optidx_t>(split[0]);
      if (Exists(tsoptidx.index)) {
        const auto idxts = idxts_t(Value(tsoptidx.index), tsoptidx.us);
        if (split.size() != 2u || idxts.index != index_) {
          CURRENT_THROW(RemoteStreamMalformedChunkException());
        }
        auto entry = ParseJSON<TYPE_SUBSCRIBED_TO>(split[1]);
        ++index_;
        if (subscriber_(std::move(entry), idxts, unused_idxts_) == ss::EntryResponse::Done) {
          CURRENT_THROW(StreamTerminatedBySubscriber());
        }
      } else {
        if (split.size() != 1u) {
          CURRENT_THROW(RemoteStreamMalformedChunkException());
        }
        if (subscriber_(tsoptidx.us) == ss::EntryResponse::Done) {
          CURRENT_THROW(StreamTerminatedBySubscriber());
        }
      }
    }

    template <ReplicationMode MODE = RM>
    ENABLE_IF<MODE == ReplicationMode::Unchecked> PassEntryToSubscriber(const std::string& raw_log_line) {
      const auto tab_pos = raw_log_line.find('\t');
      if (tab_pos != std::string::npos) {
        if (subscriber_(raw_log_line, index_++, unused_idxts_) == ss::EntryResponse::Done) {
          CURRENT_THROW(StreamTerminatedBySubscriber());
        }
      } else {
        const auto tsoptidx = ParseJSON<ts_only_t>(raw_log_line);
        if (subscriber_(tsoptidx.us) == ss::EntryResponse::Done) {
          CURRENT_THROW(StreamTerminatedBySubscriber());
        }
      }
    }
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO, ReplicationMode RM>
  class RemoteSubscriberThread final : public current::stream::SubscriberScope::SubscriberThread,
                                       public RemoteStreamSubscriber<F, TYPE_SUBSCRIBED_TO, RM> {
    using base_subscriber_t = RemoteStreamSubscriber<F, TYPE_SUBSCRIBED_TO, RM>;

   public:
    RemoteSubscriberThread(Borrowed<RemoteStream> remote_stream,
                           F& subscriber,
                           uint64_t start_idx,
                           SubscriptionMode subscription_mode,
                           std::function<void()> done_callback)
        : base_subscriber_t(remote_stream, subscriber, start_idx, [this]() { TerminateSubscription(); }),
          valid_(false),
          done_callback_(done_callback),
          subscription_mode_(subscription_mode),
          terminate_subscription_requested_(false),
          thread_([this]() { Thread(); }) {
      valid_ = true;
    }

    ~RemoteSubscriberThread() {
      if (valid_) {
        TerminateSubscription();
        thread_.join();
      } else {
        if (done_callback_) {
          done_callback_();
        }
      }
    }

   private:
    RemoteSubscriberThread() = delete;
    RemoteSubscriberThread(const RemoteSubscriberThread&) = delete;
    RemoteSubscriberThread(RemoteSubscriberThread&&) = delete;
    void operator=(const RemoteSubscriberThread&) = delete;
    void operator=(RemoteSubscriberThread&&) = delete;

    void Thread() {
      ThreadImpl();
      subscriber_thread_done_ = true;
      subscription_id_.MutableScopedAccessor()->clear();
      if (done_callback_) {
        done_callback_();
      }
    }

    void ThreadImpl() {
      const RemoteStream& bare_stream = *this->borrowed_remote_stream_;
      bool terminate_sent = false;
      while (true) {
        if (!terminate_sent && terminate_subscription_requested_) {
          terminate_sent = true;
          if (this->subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        try {
          bare_stream.CheckSchema();
          HTTP(ChunkedGET(bare_stream.GetURLToSubscribe(this->index_, subscription_mode_),
                          [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
                          [this](const std::string& chunk_body) { OnChunk(chunk_body); },
                          [this]() {}));
        } catch (StreamTerminatedBySubscriber&) {
          break;
        } catch (RemoteStreamMalformedChunkException&) {
          if (++consecutive_malformed_chunks_count_ == 3) {
            fprintf(stderr,
                    "Constantly receiving malformed chunks from \"%s\"\n",
                    bare_stream.GetURLToSubscribe(this->index_, subscription_mode_).c_str());
          }
        } catch (current::Exception&) {
        }
        this->carried_over_data_.clear();
        subscription_id_.MutableScopedAccessor()->clear();
      }
    }

    void OnHeader(const std::string& header, const std::string& value) {
      if (header == "X-Current-Stream-Subscription-Id") {
        subscription_id_.SetValue(value);
      }
    }

    void OnChunk(const std::string& chunk) {
      if (terminate_subscription_requested_) {
        return;
      }

      this->PassChunkToSubscriber(chunk);
      consecutive_malformed_chunks_count_ = 0;
    }

    void TerminateSubscription() {
      subscription_id_.Wait([this](const std::string& subscription_id) {
        if (subscriber_thread_done_) {
          return true;
        } else {
          terminate_subscription_requested_ = true;
          if (!subscription_id.empty()) {
            const std::string terminate_url = this->borrowed_remote_stream_->GetURLToTerminate(subscription_id);
            try {
              HTTP(GET(terminate_url));
            } catch (current::Exception&) {
            }
            return true;
          }
        }
        return false;
      });
    }

   private:
    bool valid_;
    const std::function<void()> done_callback_;
    const SubscriptionMode subscription_mode_;
    current::WaitableAtomic<std::string> subscription_id_;
    std::atomic_bool terminate_subscription_requested_;
    std::thread thread_;
    uint32_t consecutive_malformed_chunks_count_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO, ReplicationMode RM>
  class RemoteSubscriberScopeImpl final : public current::stream::SubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using base_t = current::stream::SubscriberScope;

   public:
    using subscriber_thread_t = RemoteSubscriberThread<F, TYPE_SUBSCRIBED_TO, RM>;

    RemoteSubscriberScopeImpl(Borrowed<RemoteStream> remote_stream,
                              F& subscriber,
                              uint64_t start_idx,
                              SubscriptionMode subscription_mode,
                              std::function<void()> done_callback)
        : base_t(std::move(std::make_unique<subscriber_thread_t>(
              std::move(remote_stream), subscriber, start_idx, subscription_mode, done_callback))) {}
  };
  template <typename F>
  using RemoteSubscriberScope = RemoteSubscriberScopeImpl<F, entry_t, ReplicationMode::Checked>;
  template <typename F>
  using RemoteSubscriberScopeUnchecked = RemoteSubscriberScopeImpl<F, entry_t, ReplicationMode::Unchecked>;

  explicit SubscribableRemoteStream(const std::string& remote_stream_url)
      : stream_(MakeOwned<RemoteStream>(
            remote_stream_url, stream::constants::kDefaultTopLevelName, stream::constants::kDefaultNamespaceName)) {
    stream_->CheckSchema();
  }

  explicit SubscribableRemoteStream(const std::string& remote_stream_url,
                                    const std::string& entry_name,
                                    const std::string& namespace_name)
      : stream_(MakeOwned<RemoteStream>(remote_stream_url, entry_name, namespace_name)) {
    stream_->CheckSchema();
  }

  template <typename F>
  RemoteSubscriberScope<F> Subscribe(F& subscriber,
                                     uint64_t start_idx = 0u,
                                     SubscriptionMode subscription_mode = SubscriptionMode::Unchecked,
                                     std::function<void()> done_callback = nullptr) const {
    static_assert(current::ss::IsStreamSubscriber<F, entry_t>::value, "");
    return RemoteSubscriberScope<F>(stream_, subscriber, start_idx, subscription_mode, done_callback);
  }

  template <typename F, ReplicationMode RM>
  void FlipToMaster(F& subscriber, head_optidxts_t head_idxts, uint64_t key, SubscriptionMode subscription_mode) const {
    static_assert(current::ss::IsStreamSubscriber<F, entry_t>::value, "");
    const auto response = HTTP(GET(stream_->GetFlipToMasterURL(head_idxts, key, subscription_mode)));
    if (response.code == HTTPResponseCode.OK) {
      const auto start_idx = Exists(head_idxts.idxts) ? Value(head_idxts.idxts).index + 1 : 0;
      RemoteStreamSubscriber<F, entry_t, RM> remote_subscriber(stream_, subscriber, start_idx, []() {});
      remote_subscriber.PassChunkToSubscriber(response.body);
    } else {
      const auto response_string = ToString(response.code) + " " + HTTPResponseCodeAsString(response.code);
      CURRENT_THROW(RemoteStreamRefusedFlipRequestException(response_string));
    }
  }

  template <typename F>
  RemoteSubscriberScopeUnchecked<F> SubscribeUnchecked(F& subscriber,
                                                       uint64_t start_idx = 0u,
                                                       SubscriptionMode subscription_mode = SubscriptionMode::Unchecked,
                                                       std::function<void()> done_callback = nullptr) const {
    static_assert(current::ss::IsStreamSubscriber<F, entry_t>::value, "");
    return RemoteSubscriberScopeUnchecked<F>(stream_, subscriber, start_idx, subscription_mode, done_callback);
  }

  uint64_t GetNumberOfEntries() const {
    return stream_.ObjectAccessorDespitePossiblyDestructing().GetNumberOfEntries();
  }

 private:
  Owned<RemoteStream> stream_;
};

template <typename STREAM>
struct StreamReplicatorImpl {
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;
  using stream_t = STREAM;
  using entry_t = typename stream_t::entry_t;
  using publisher_t = typename stream_t::publisher_t;

  StreamReplicatorImpl(Borrowed<stream_t> stream) : publisher_(std::move(stream->BecomeFollowingStream())) {}
  StreamReplicatorImpl(Borrowed<publisher_t>&& publisher) : publisher_(std::move(publisher)) {}
  virtual ~StreamReplicatorImpl() {
    publisher_ = nullptr;
    // NOTE(dkorolev): The destructor should not automatically order the stream to re-acquire data authority.
    // Otherwise the stream will SUDDENLY become the master one again, w/o any action from the user. Not cool.
    // The user should be responsible for restoring the stream's data authority as an instance
    // of `StreamReplicator` is being destructed.
    // NOTE(dkorolev): Master flip logic also plays well here.
  }

  EntryResponse operator()(entry_t&& entry, idxts_t current, idxts_t) {
    Value(publisher_)->Publish(std::move(entry), current.us);
    return EntryResponse::More;
  }

  EntryResponse operator()(const entry_t& entry, idxts_t current, idxts_t) {
    Value(publisher_)->Publish(entry, current.us);
    return EntryResponse::More;
  }

  EntryResponse operator()(std::string&& raw_log_line, uint64_t, idxts_t) {
    Value(publisher_)->PublishUnsafe(std::move(raw_log_line));
    return EntryResponse::More;
  }

  EntryResponse operator()(const std::string& raw_log_line, uint64_t, idxts_t) {
    Value(publisher_)->PublishUnsafe(raw_log_line);
    return EntryResponse::More;
  }

  EntryResponse operator()(std::chrono::microseconds ts) {
    Value(publisher_)->UpdateHead(ts);
    return EntryResponse::More;
  }

  EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
  TerminationResponse Terminate() const { return TerminationResponse::Terminate; }

 private:
  // `Optional` to destroy the publisher before requesting the stream to reacquire data authority.
  Optional<Borrowed<publisher_t>> publisher_;
};

template <typename STREAM>
using StreamReplicator = current::ss::StreamSubscriber<StreamReplicatorImpl<STREAM>, typename STREAM::entry_t>;

struct MasterFlipRestrictions {
  uint64_t max_index_diff_ = 0;
  uint64_t max_diff_size_ = 0;
  std::chrono::microseconds max_head_diff_ = std::chrono::microseconds(0);
  std::chrono::microseconds max_clock_diff_ = std::chrono::microseconds(0);

  MasterFlipRestrictions& SetMaxIndexDifference(uint64_t value) {
    max_index_diff_ = value;
    return *this;
  }
  MasterFlipRestrictions& SetMaxHeadDifference(std::chrono::microseconds value) {
    max_head_diff_ = value;
    return *this;
  }
  MasterFlipRestrictions& SetMaxClockDifference(std::chrono::microseconds value) {
    max_clock_diff_ = value;
    return *this;
  }
  MasterFlipRestrictions& SetMaxDiffSize(uint64_t bytes) {
    max_diff_size_ = bytes;
    return *this;
  }
};

template <typename STREAM,
          typename REPLICATOR = StreamReplicator<STREAM>,
          ReplicationMode RM = ReplicationMode::Checked>
class MasterFlipController final {
 public:
  using stream_t = STREAM;
  using replicator_t = REPLICATOR;
  using entry_t = typename stream_t::entry_t;
  using publisher_t = typename stream_t::publisher_t;

  MasterFlipController(Owned<STREAM>&& stream) : stream_(std::move(stream)) {}

  uint64_t ExposeViaHTTP(uint16_t port,
                         const std::string& route,
                         MasterFlipRestrictions restrictions = MasterFlipRestrictions(),
                         std::function<void()> flip_started = nullptr,
                         std::function<void()> flip_finished = nullptr,
                         std::function<void()> flip_canceled = nullptr) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (exposed_via_http_) {
      CURRENT_THROW(StreamIsAlreadyExposedException());
    }

    exposed_via_http_ = std::make_unique<ExposedStreamState>(
        port, route, std::move(restrictions), flip_started, flip_finished, flip_canceled);
    exposed_via_http_->routes_scope_ +=
        HTTP(port).Register(route, URLPathArgs::CountMask::None | URLPathArgs::CountMask::One, *Value(stream_)) +
        HTTP(port).Register(
            route + "/become/master", URLPathArgs::CountMask::None, [this](Request r) { MasterFlipRequest(r); });
    return exposed_via_http_->flip_key_;
  }

  void StopExposingViaHTTP(uint16_t port, const std::string& route) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (exposed_via_http_ && exposed_via_http_->port_ == port && exposed_via_http_->route_ == route) {
      exposed_via_http_ = nullptr;
    } else {
      CURRENT_THROW(StreamIsNotExposedException());
    }
  }

  void FollowRemoteStream(const std::string& url, SubscriptionMode subscription_mode = SubscriptionMode::Unchecked) {
    if (remote_follower_) {
      CURRENT_THROW(StreamIsAlreadyFollowingException());
    }

    std::unique_lock<std::mutex> lock(mutex_);
    const bool has_borrowed_publisher = Exists(borrowed_publisher_);
    try {
      remote_follower_ = std::make_unique<RemoteStreamFollower>(
          has_borrowed_publisher ? std::move(Value(borrowed_publisher_)) : stream_->BecomeFollowingStream(),
          url,
          stream_->Data()->Size(),
          subscription_mode);
      borrowed_publisher_ = nullptr;
    } catch (current::Exception& e) {
      // Can't follow the remote stream for some reason,
      // restore the stream state and propagate the exception.
      if (has_borrowed_publisher)
        borrowed_publisher_ = stream_->BecomeFollowingStream();
      else
        stream_->BecomeMasterStream();
      throw;
    }
  }

  void FlipToMaster(uint64_t key) {
    if (stream_->IsMasterStream()) {
      CURRENT_THROW(StreamIsAlreadyMasterException());
    }
    if (!remote_follower_) {
      CURRENT_THROW(StreamDoesNotFollowAnyoneException());
    }
    remote_follower_->PerformMasterFlip(*Value(stream_), key);
    remote_follower_ = nullptr;
    stream_->BecomeMasterStream();
  }

  bool IsMasterStream() const { return stream_->IsMasterStream(); }
  stream_t& Stream() { return *Value(stream_); }
  const stream_t& Stream() const { return *Value(stream_); }
  stream_t& operator*() { return *Value(stream_); }
  const stream_t& operator*() const { return *Value(stream_); }
  stream_t* operator->() { return stream_.operator->(); }
  const stream_t* operator->() const { return stream_.operator->(); }

 private:
  void MasterFlipRequest(Request& r) {
    if (r.method != "GET") {
      r(current::net::DefaultMethodNotAllowedMessage(), HTTPResponseCode.MethodNotAllowed);
      return;
    }

    if (!r.url.query.has("head") || !r.url.query.has("key")) {
      r("", HTTPResponseCode.BadRequest);
      return;
    }
    const bool has_i = r.url.query.has("i");
    const auto client_head = std::chrono::microseconds(current::FromString<int64_t>(r.url.query["head"]));
    const auto next_index = has_i ? current::FromString<uint64_t>(r.url.query["i"]) + 1 : 0;
    const auto flip_key = current::FromString<uint64_t>(r.url.query["key"]);

    const auto head_idxts = stream_->Data()->HeadAndLastPublishedIndexAndTimestamp();
    if (client_head > head_idxts.head) {
      r("", HTTPResponseCode.BadRequest);
      return;
    }
    if (has_i && (!Exists(head_idxts.idxts) || next_index > Value(head_idxts.idxts).index + 1)) {
      r("", HTTPResponseCode.BadRequest);
      return;
    }

    std::unique_lock<std::mutex> lock(mutex_);
    if (!exposed_via_http_ || Exists(borrowed_publisher_)) {
      r("", HTTPResponseCode.ServiceUnavailable);
      return;
    }
    const auto& restrictions = exposed_via_http_->flip_restrictions_;
    const auto max_clock_diff = restrictions.max_clock_diff_.count();
    if (max_clock_diff > 0 &&
        (!r.url.query.has("clock") ||
         abs(current::FromString<int64_t>(r.url.query["clock"]) - current::time::Now().count()) > max_clock_diff)) {
      r("", HTTPResponseCode.BadRequest);
      return;
    }
    if (remote_follower_) {
      try {
        remote_follower_->PerformMasterFlip(*Value(stream_), flip_key);
        remote_follower_ = nullptr;
        stream_->BecomeMasterStream();
      } catch (RemoteStreamRefusedFlipRequestException&) {
        r("", HTTPResponseCode.BadRequest);
        return;
      }
    } else if (flip_key != exposed_via_http_->flip_key_) {
      r("", HTTPResponseCode.BadRequest);
      return;
    }

    if (exposed_via_http_->flip_started_callback_) {
      exposed_via_http_->flip_started_callback_();
    }

    const bool checked = r.url.query.has("checked");
    // Grab the publisher to make sure no one can publish into it.
    borrowed_publisher_ = stream_->BecomeFollowingStream();

    // Send back the remaining diff.
    const auto succeeded = ValidateAndSendDiff(r, next_index, client_head, checked);
    if (!succeeded) {
      // If something went wrong, restore the original state.
      borrowed_publisher_ = nullptr;
      stream_->BecomeMasterStream();
      if (exposed_via_http_->flip_canceled_callback_) exposed_via_http_->flip_canceled_callback_();
    } else if (exposed_via_http_->flip_finished_callback_) {
      // Otherwise notify about flip completion from a separate thread
      // to make it possible to unregister endpoints
      // or even destroy the stream from inside the callback.
      std::thread(exposed_via_http_->flip_finished_callback_).detach();
    }
  }

  bool ValidateAndSendDiff(Request& r,
                           uint64_t start_idx,
                           std::chrono::microseconds head_us,
                           bool checked_subscription) const {
    const auto head_idxts = stream_->Data()->HeadAndLastPublishedIndexAndTimestamp();
    const auto& restrictions = exposed_via_http_->flip_restrictions_;
    const auto max_index_diff = restrictions.max_index_diff_;
    const auto max_diff_size = restrictions.max_diff_size_;
    const auto max_head_diff = restrictions.max_head_diff_;

    if (max_index_diff != 0 && start_idx != 0 && Value(head_idxts.idxts).index > start_idx + max_index_diff - 1) {
      r("", HTTPResponseCode.BadRequest);
      return false;
    }
    if (max_head_diff.count() > 0 && head_idxts.head > head_us + max_head_diff) {
      r("", HTTPResponseCode.BadRequest);
      return false;
    }

    std::stringstream sstream(std::ios::out | std::ios::app);
    if (checked_subscription) {
      for (const auto& e : stream_->Data()->Iterate(start_idx)) {
        if (head_us >= e.idx_ts.us) {
          r("", HTTPResponseCode.BadRequest);
          return false;
        }
        sstream << JSON(e.idx_ts) << '\t' << JSON(e.entry) << '\n';
        if (max_diff_size != 0 && static_cast<unsigned long long>(sstream.tellp()) > max_diff_size) {
          r("", HTTPResponseCode.BadRequest);
          return false;
        }
      }
    } else {
      auto us_to_check_with = head_us;
      for (const auto& e : stream_->Data()->IterateUnsafe(start_idx)) {
        if (us_to_check_with.count() >= 0 && us_to_check_with >= ParseJSON<idxts_t>(e).us) {
          r("", HTTPResponseCode.BadRequest);
          return false;
        }
        us_to_check_with = std::chrono::microseconds(-1);
        sstream << e << '\n';
        if (max_diff_size != 0 && static_cast<unsigned long long>(sstream.tellp()) > max_diff_size) {
          r("", HTTPResponseCode.BadRequest);
          return false;
        }
      }
    }
    if (head_idxts.head > head_us && (!Exists(head_idxts.idxts) || head_idxts.head > Value(head_idxts.idxts).us))
      sstream << JSON<JSONFormat::Minimalistic>(ts_only_t(head_idxts.head)) << '\n';
    r(sstream.str(), HTTPResponseCode.OK);
    return true;
  }

  struct ExposedStreamState {
    uint16_t port_;
    std::string route_;
    uint64_t flip_key_;
    MasterFlipRestrictions flip_restrictions_;
    std::function<void()> flip_started_callback_;
    std::function<void()> flip_finished_callback_;
    std::function<void()> flip_canceled_callback_;
    HTTPRoutesScope routes_scope_;

    ExposedStreamState(uint16_t port,
                       const std::string& route,
                       MasterFlipRestrictions&& restrictions,
                       std::function<void()> flip_started,
                       std::function<void()> flip_finished,
                       std::function<void()> flip_canceled)
        : port_(port),
          route_(route),
          flip_key_(random::CSRandomUInt64(0llu, ~0llu)),
          flip_restrictions_(std::move(restrictions)),
          flip_started_callback_(flip_started),
          flip_finished_callback_(flip_finished),
          flip_canceled_callback_(flip_canceled) {}
  };

  struct RemoteStreamFollower {
    using remote_stream_t = SubscribableRemoteStream<entry_t>;
    SubscriptionMode subscription_mode_;
    remote_stream_t remote_stream_;
    replicator_t replicator_;
    std::unique_ptr<SubscriberScope> subscriber_scope_;

    RemoteStreamFollower(Borrowed<publisher_t>&& publisher,
                         const std::string& url,
                         uint64_t start_idx,
                         SubscriptionMode subscription_mode)
        : subscription_mode_(subscription_mode),
          remote_stream_(url),
          replicator_(std::move(publisher)),
          subscriber_scope_(Subscribe(start_idx)) {}

    void PerformMasterFlip(const stream_t& stream, uint64_t key) {
      // terminate the remote subscription.
      subscriber_scope_ = nullptr;
      // force the remote stream to send the rest of its data and destroy itself.
      try {
        remote_stream_.template FlipToMaster<replicator_t, RM>(
            replicator_, stream.Data()->HeadAndLastPublishedIndexAndTimestamp(), key, subscription_mode_);
      } catch (current::Exception& e) {
        // restore the subscription if the flip failed.
        subscriber_scope_ = Subscribe(stream.Data()->Size());
        throw;
      }
    }

   private:
    template <ReplicationMode MODE = RM>
    ENABLE_IF<MODE == ReplicationMode::Checked, std::unique_ptr<SubscriberScope>> Subscribe(uint64_t start_idx) {
      using scope_t = typename remote_stream_t::template RemoteSubscriberScope<replicator_t>;
      return std::make_unique<scope_t>(remote_stream_.Subscribe(replicator_, start_idx, subscription_mode_));
    }
    template <ReplicationMode MODE = RM>
    ENABLE_IF<MODE == ReplicationMode::Unchecked, std::unique_ptr<SubscriberScope>> Subscribe(uint64_t start_idx) {
      using scope_t = typename remote_stream_t::template RemoteSubscriberScopeUnchecked<replicator_t>;
      return std::make_unique<scope_t>(remote_stream_.SubscribeUnchecked(replicator_, start_idx, subscription_mode_));
    }
  };

  Owned<stream_t> stream_;
  Optional<Borrowed<publisher_t>> borrowed_publisher_;
  std::unique_ptr<ExposedStreamState> exposed_via_http_;
  std::unique_ptr<RemoteStreamFollower> remote_follower_;
  std::mutex mutex_;
};

}  // namespace stream
}  // namespace current

#endif  // CURRENT_STREAM_REPLICATOR_H
