/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Grigory Nikolaenko <nikolaenko.grigory@gmail.com>

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

#ifndef CURRENT_SHERLOCK_REPLICATOR_H
#define CURRENT_SHERLOCK_REPLICATOR_H

#include <functional>
#include <string>
#include <thread>

#include "exceptions.h"
#include "sherlock.h"
#include "stream_data.h"

#include "../Blocks/HTTP/api.h"
#include "../Blocks/SS/ss.h"

#include "../Bricks/sync/scope_owned.h"
#include "../Bricks/sync/waitable_atomic.h"

#include "../TypeSystem/Reflection/types.h"

namespace current {
namespace sherlock {

template <typename STREAM_ENTRY>
class SubscribableRemoteStream final {
 public:
  using entry_t = STREAM_ENTRY;

  class RemoteStream final {
   public:
    RemoteStream(const std::string& url, const std::string& top_level_name, const std::string& namespace_name)
        : url_(url),
          schema_(Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<entry_t>()).type_id,
                  top_level_name,
                  namespace_name) {}

    void CheckSchema() const {
      const auto response = HTTP(GET(url_ + "/schema.simple"));
      if (response.code == HTTPResponseCode.OK) {
        const auto remote_schema = ParseJSON<SubscribableSherlockSchema>(response.body);
        if (remote_schema != schema_) {
          CURRENT_THROW(RemoteStreamInvalidSchemaException());
        }
      } else {
        CURRENT_THROW(RemoteStreamDoesNotRespondException());
      }
    }

    std::string GetURLToSubscribe(uint64_t index) const { return url_ + "?i=" + current::ToString(index); }

    std::string GetURLToTerminate(const std::string& subscription_id) const {
      return url_ + "?terminate=" + subscription_id;
    }

   private:
    const std::string url_;
    const SubscribableSherlockSchema schema_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO>
  class RemoteSubscriberThread final : public current::sherlock::SubscriberScope::SubscriberThread {
    static_assert(current::ss::IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");

   public:
    RemoteSubscriberThread(ScopeOwned<RemoteStream>& remote_stream,
                           F& subscriber,
                           uint64_t start_idx,
                           std::function<void()> done_callback)
        : valid_(false),
          remote_stream_(remote_stream, [this]() { TerminateSubscription(); }),
          done_callback_(done_callback),
          subscriber_(subscriber),
          index_(start_idx),
          unused_idxts_(),
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
      const RemoteStream& bare_stream = remote_stream_.ObjectAccessorDespitePossiblyDestructing();
      bool terminate_sent = false;
      while (true) {
        if (!terminate_sent && terminate_subscription_requested_) {
          terminate_sent = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        try {
          bare_stream.CheckSchema();
          HTTP(ChunkedGET(bare_stream.GetURLToSubscribe(index_),
                          [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
                          [this](const std::string& chunk_body) { OnChunk(chunk_body); },
                          [this]() {}));
        } catch (StreamTerminatedBySubscriber&) {
          break;
        } catch (current::Exception&) {
        }
        carried_over_data_.clear();
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

      const std::string combined_data = carried_over_data_ + chunk;
      const auto lines = current::strings::Split<current::strings::ByLines>(combined_data);
      size_t whole_entries_count = lines.size();
      CURRENT_ASSERT(!combined_data.empty());
      if (combined_data.back() != '\n' && combined_data.back() != '\r') {
        --whole_entries_count;
        carried_over_data_ = lines.back();
      } else {
        carried_over_data_.clear();
      }
      for (size_t i = 0; i < whole_entries_count; ++i) {
        const auto split = current::strings::Split(lines[i], '\t');
        CURRENT_ASSERT(split.size() == 1u || split.size() == 2u);
        if (split.size() == 2u) {
          const auto idxts = ParseJSON<idxts_t>(split[0]);
          CURRENT_ASSERT(idxts.index == index_);
          auto entry = ParseJSON<TYPE_SUBSCRIBED_TO>(split[1]);
          ++index_;
          if (subscriber_(std::move(entry), idxts, unused_idxts_) == ss::EntryResponse::Done) {
            CURRENT_THROW(StreamTerminatedBySubscriber());
          }
        } else if (split.size() == 1u) {
          const auto tsoptidx = ParseJSON<ts_optidx_t>(split[0]);
          if (subscriber_(tsoptidx.us) == ss::EntryResponse::Done) {
            CURRENT_THROW(StreamTerminatedBySubscriber());
          }
        }
      }
    }

    void TerminateSubscription() {
      subscription_id_.Wait([this](const std::string& subscription_id) {
        if (subscriber_thread_done_ || terminate_subscription_requested_) {
          return true;
        } else if (!subscription_id.empty()) {
          terminate_subscription_requested_ = true;
          const std::string terminate_url =
              remote_stream_.ObjectAccessorDespitePossiblyDestructing().GetURLToTerminate(subscription_id);
          try {
            HTTP(GET(terminate_url));
          } catch (current::Exception&) {
          }
          return true;
        }
        return false;
      });
    }

   private:
    bool valid_;
    ScopeOwnedBySomeoneElse<RemoteStream> remote_stream_;
    const std::function<void()> done_callback_;
    F& subscriber_;
    uint64_t index_;
    const idxts_t unused_idxts_;
    current::WaitableAtomic<std::string> subscription_id_;
    std::atomic_bool terminate_subscription_requested_;
    std::thread thread_;
    std::string carried_over_data_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO>
  class RemoteSubscriberScope final : public current::sherlock::SubscriberScope {
   private:
    static_assert(current::ss::IsStreamSubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    using base_t = current::sherlock::SubscriberScope;

   public:
    using subscriber_thread_t = RemoteSubscriberThread<F, TYPE_SUBSCRIBED_TO>;

    RemoteSubscriberScope(ScopeOwned<RemoteStream>& remote_stream,
                          F& subscriber,
                          uint64_t start_idx,
                          std::function<void()> done_callback)
        : base_t(
              std::move(std::make_unique<subscriber_thread_t>(remote_stream, subscriber, start_idx, done_callback))) {}
  };

  explicit SubscribableRemoteStream(const std::string& remote_stream_url)
      : stream_(
            remote_stream_url, sherlock::constants::kDefaultTopLevelName, sherlock::constants::kDefaultNamespaceName) {
    stream_.ObjectAccessorDespitePossiblyDestructing().CheckSchema();
  }

  explicit SubscribableRemoteStream(const std::string& remote_stream_url,
                                    const std::string& top_level_name,
                                    const std::string& namespace_name)
      : stream_(remote_stream_url, top_level_name, namespace_name) {
    stream_.ObjectAccessorDespitePossiblyDestructing().CheckSchema();
  }

  template <typename F>
  RemoteSubscriberScope<F, entry_t> Subscribe(F& subscriber,
                                              uint64_t start_idx = 0u,
                                              std::function<void()> done_callback = nullptr) {
    static_assert(current::ss::IsStreamSubscriber<F, entry_t>::value, "");
    try {
      return RemoteSubscriberScope<F, entry_t>(stream_, subscriber, start_idx, done_callback);
    } catch (const current::sync::InDestructingModeException&) {
      CURRENT_THROW(StreamInGracefulShutdownException());
    }
  }

 private:
  ScopeOwnedByMe<RemoteStream> stream_;
};

template <typename STREAM>
struct StreamReplicatorImpl {
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;
  using stream_t = STREAM;
  using entry_t = typename stream_t::entry_t;
  using publisher_t = typename stream_t::publisher_t;

  StreamReplicatorImpl(stream_t& stream) : stream_(stream) { stream.MovePublisherTo(*this); }
  virtual ~StreamReplicatorImpl() { stream_.AcquirePublisher(std::move(publisher_)); }

  void AcceptPublisher(std::unique_ptr<publisher_t> publisher) { publisher_ = std::move(publisher); }

  EntryResponse operator()(entry_t&& entry, idxts_t current, idxts_t) {
    CURRENT_ASSERT(publisher_);
    publisher_->Publish(std::move(entry), current.us);
    return EntryResponse::More;
  }

  EntryResponse operator()(const entry_t& entry, idxts_t current, idxts_t) {
    CURRENT_ASSERT(publisher_);
    publisher_->Publish(entry, current.us);
    return EntryResponse::More;
  }

  EntryResponse operator()(std::chrono::microseconds ts) {
    CURRENT_ASSERT(publisher_);
    publisher_->UpdateHead(ts);
    return EntryResponse::More;
  }

  EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return EntryResponse::More; }
  TerminationResponse Terminate() const { return TerminationResponse::Terminate; }

 private:
  stream_t& stream_;
  std::unique_ptr<publisher_t> publisher_;
};

template <typename STREAM>
using StreamReplicator = current::ss::StreamSubscriber<StreamReplicatorImpl<STREAM>, typename STREAM::entry_t>;

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_REPLICATOR_H
