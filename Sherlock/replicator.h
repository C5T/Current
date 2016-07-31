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
      if (static_cast<int>(response.code) == 200) {
        const auto remote_schema = ParseJSON<SubscribableSherlockSchema>(response.body);
        if (remote_schema != schema_) {
          CURRENT_THROW(RemoteStreamInvalidSchemaException());
        }
      } else {
        CURRENT_THROW(RemoteStreamDoesNotRespondException());
      }
    }

    const std::string GetURLToSubscribe(uint64_t index) const {
      return url_ + "?i=" + current::ToString(index);
    }

    const std::string GetURLToTerminate(const std::string& subscription_id) {
      return url_ + "?terminate=" + subscription_id;
    }

   private:
    const std::string url_;
    const SubscribableSherlockSchema schema_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO>
  class RemoteSubscriberThread final : public current::sherlock::SubscriberScope::SubscriberThread {
   public:
    RemoteSubscriberThread(ScopeOwned<RemoteStream>& remote_stream,
                           F& subscriber,
                           uint64_t start_idx,
                           std::function<void()> done_callback)
        : valid_(false),
          done_callback_(done_callback),
          external_stop_(false),
          remote_stream_(remote_stream, [this]() { TerminateSubscription(); }),
          subscriber_(subscriber),
          index_(start_idx),
          thread_([this]() { Thread(); }) {
      valid_ = true;
    }

    ~RemoteSubscriberThread() {
      if (valid_) {
        if (!subscriber_thread_done_) {
          TerminateSubscription();
        }
        thread_.join();
      } else {
        if (done_callback_) {
          done_callback_();
        }
      }
    }

   private:
    static_assert(current::ss::IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    RemoteSubscriberThread() = delete;
    RemoteSubscriberThread(const RemoteSubscriberThread&) = delete;
    RemoteSubscriberThread(RemoteSubscriberThread&&) = delete;
    void operator=(const RemoteSubscriberThread&) = delete;
    void operator=(RemoteSubscriberThread&&) = delete;

    void Thread() {
      ThreadImpl();
      subscriber_thread_done_ = true;
      if (done_callback_) {
        done_callback_();
      }
    }

    void ThreadImpl() {
      const RemoteStream& bare_stream = remote_stream_.ObjectAccessorDespitePossiblyDestructing();
      bool terminate_sent = false;
      while (true) {
        if (!terminate_sent && external_stop_) {
          terminate_sent = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        try {
          bare_stream.CheckSchema();
          HTTP(ChunkedGET(
              bare_stream.GetURLToSubscribe(index_),
              [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
              [this](const std::string& chunk_body) { OnChunk(chunk_body); },
              [this]() {}));
        } catch (StreamTerminatedBySubscriber&) {
          break;
        } catch (current::Exception&) {
        }
      }
    }

    void OnHeader(const std::string& header, const std::string& value) {
      if (header == "X-Current-Stream-Subscription-Id") {
        std::unique_lock<std::mutex> lock(mutex_);
        subscription_id_ = value;
        event_.notify_one();
      }
    }

    void OnChunk(const std::string& chunk) {
      if (external_stop_ ) {
        return;
      }

      // Expected one entire entry in each chunk,
      // won't work in case of partial or multiple entries per chunk
      const auto split = current::strings::Split(chunk, '\t');
      assert(split.size() == 2u);
      const auto idxts = ParseJSON<idxts_t>(split[0]);
      assert(idxts.index == index_);
      auto entry = ParseJSON<TYPE_SUBSCRIBED_TO>(split[1]);
      const idxts_t unused_idxts;
      ++index_;
      if (subscriber_(std::move(entry), idxts, unused_idxts) == ss::EntryResponse::Done) {
        CURRENT_THROW(StreamTerminatedBySubscriber());
      }
    }

    void TerminateSubscription() {
      std::unique_lock<std::mutex> lock(mutex_);
      event_.wait(lock, [this]() { return !subscription_id_.empty() || subscriber_thread_done_; });
      external_stop_ = true;
      if (!subscription_id_.empty()) {
        const std::string terminate_url =
            remote_stream_.ObjectAccessorDespitePossiblyDestructing().GetURLToTerminate(subscription_id_);
        try {
          const auto result = HTTP(GET(terminate_url));
        } catch (current::net::NetworkException&) {
        }
      }
    }

   private:
    bool valid_;
    std::function<void()> done_callback_;
    std::atomic_bool external_stop_;
    ScopeOwnedBySomeoneElse<RemoteStream> remote_stream_;
    F& subscriber_;
    uint64_t index_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable event_;
    std::string subscription_id_;
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
        : base_t(std::move(
              std::make_unique<subscriber_thread_t>(remote_stream, subscriber, start_idx, done_callback))) {}
  };

  explicit SubscribableRemoteStream(const std::string& remote_stream_url)
      : stream_(remote_stream_url,
                sherlock::constants::kDefaultTopLevelName,
                sherlock::constants::kDefaultNamespaceName) {
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
    assert(publisher_);
    publisher_->Publish(std::move(entry), current.us);
    return EntryResponse::More;
  }

  EntryResponse operator()(const entry_t& entry, idxts_t current, idxts_t) {
    assert(publisher_);
    publisher_->Publish(entry, current.us);
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
