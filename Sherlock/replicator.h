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

#ifndef REMOTE_STREAM_REPLICATOR_H
#define REMOTE_STREAM_REPLICATOR_H

namespace current {
namespace sherlock {

template <typename T_STREAM_ENTRY>
class SubscribableRemoteStream final {
 public:
  struct RemoteStream final {
    RemoteStream(const std::string& url) : url_(url) {}

    const std::string url_;
  };

  template <typename F, typename TYPE_SUBSCRIBED_TO>
  class RemoteSubscriberThread final : public current::sherlock::SubscriberScope::SubscriberThread {
   private:
    bool valid_;
    std::function<void()> done_callback_;
    std::atomic_bool external_stop_;
    std::atomic_bool internal_stop_;
    ScopeOwnedBySomeoneElse<RemoteStream> remote_stream_;
    F& subscriber_;
    uint64_t index_;
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable event_;
    std::string subscription_id_;

    RemoteSubscriberThread() = delete;
    RemoteSubscriberThread(const RemoteSubscriberThread&) = delete;
    RemoteSubscriberThread(RemoteSubscriberThread&&) = delete;
    void operator=(const RemoteSubscriberThread&) = delete;
    void operator=(RemoteSubscriberThread&&) = delete;

   public:
    RemoteSubscriberThread(ScopeOwned<RemoteStream>& remote_stream,
                           F& subscriber,
                           uint64_t start_idx,
                           std::function<void()> done_callback)
        : valid_(false),
          done_callback_(done_callback),
          external_stop_(false),
          internal_stop_(false),
          remote_stream_(remote_stream,
                         [this]() {
                           TerminateSubscription();
                           thread_.join();
                         }),
          subscriber_(subscriber),
          index_(start_idx),
          thread_([this]() { Thread(); }) {
      static_assert(current::ss::IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
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
    void Thread() {
      ThreadImpl();
      if (done_callback_) {
        done_callback_();
      }
    }

    void ThreadImpl() {
      const std::string& url = remote_stream_.ObjectAccessorDespitePossiblyDestructing().url_;
      bool terminate_sent = false;
      while (!internal_stop_) {
        if (!terminate_sent && external_stop_) {
          terminate_sent = true;
          if (subscriber_.Terminate() != ss::TerminationResponse::Wait) {
            return;
          }
        }
        try {
          HTTP(ChunkedGET(
              url + "?i=" + current::ToString(index_),
              [this](const std::string& header, const std::string& value) { OnHeader(header, value); },
              [this](const std::string& chunk_body) { OnChunk(chunk_body); },
              [this]() {}));
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
      if (external_stop_ || internal_stop_) {
        return;
      }
      std::unique_lock<std::mutex> lock(mutex_);

      const auto split = current::strings::Split(chunk, '\t');
      assert(split.size() == 2u);
      const auto idx = ParseJSON<idxts_t>(split[0]);
      assert(idx.index == index_);
      auto entry = ParseJSON<TYPE_SUBSCRIBED_TO>(split[1]);
      internal_stop_ =
          (subscriber_(std::move(entry), idx, idxts_t() /*unused "last" arg*/) == ss::EntryResponse::Done);
      ++index_;
      event_.notify_one();
    }

    void TerminateSubscription() {
      std::unique_lock<std::mutex> lock(mutex_);
      event_.wait(lock, [this]() { return !subscription_id_.empty(); });
      external_stop_ = true;
      const std::string terminate_url = remote_stream_->url_ + "?terminate=" + subscription_id_;
      try {
        const auto result = HTTP(GET(terminate_url));
      } catch (current::net::NetworkException&) {
      }
    }
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
      : stream_(remote_stream_url),
        schema_(
            Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<T_STREAM_ENTRY>()).type_id,
            reflection::CurrentTypeName<T_STREAM_ENTRY>(),
            sherlock::constants::kDefaultTopLevelName,
            sherlock::constants::kDefaultNamespaceName) {
    CheckRemoteSchema();
  }

  explicit SubscribableRemoteStream(const std::string& remote_stream_url,
                                    const std::string& top_level_name,
                                    const std::string& namespace_name)
      : stream_(remote_stream_url),
        schema_(
            Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<T_STREAM_ENTRY>()).type_id,
            reflection::CurrentTypeName<T_STREAM_ENTRY>(),
            top_level_name,
            namespace_name) {
    CheckRemoteSchema();
  }
  ~SubscribableRemoteStream(){};

  template <typename F>
  RemoteSubscriberScope<F, T_STREAM_ENTRY> Subscribe(F& subscriber,
                                                     uint64_t start_idx = 0u,
                                                     std::function<void()> done_callback = nullptr) {
    static_assert(current::ss::IsStreamSubscriber<F, T_STREAM_ENTRY>::value, "");
    try {
      return RemoteSubscriberScope<F, T_STREAM_ENTRY>(stream_, subscriber, start_idx, done_callback);
    } catch (const current::sync::InDestructingModeException&) {
      CURRENT_THROW(StreamInGracefulShutdownException());
    }
  }

 private:
  void CheckRemoteSchema() {
    const auto response = HTTP(GET(stream_->url_ + "/schema.simple"));
    if (static_cast<int>(response.code) == 200) {
      sherlock::SubscribableSherlockSchema remote_schema =
          ParseJSON<sherlock::SubscribableSherlockSchema>(response.body);
      if (remote_schema != schema_) {
        CURRENT_THROW(RemoteStreamSchemaInvalidException());
      }
    } else {
      CURRENT_THROW(RemoteStreamDoesNotRespondException());
    }
  }

  ScopeOwnedByMe<RemoteStream> stream_;
  sherlock::SubscribableSherlockSchema schema_;
};

template <typename T_STREAM>
struct StreamReplicatorImpl {
  using EntryResponse = current::ss::EntryResponse;
  using TerminationResponse = current::ss::TerminationResponse;
  using entry_t = typename T_STREAM::entry_t;
  using publisher_t = typename T_STREAM::publisher_t;

  StreamReplicatorImpl(T_STREAM& stream) : stream_(stream) { stream.MovePublisherTo(*this); }
  ~StreamReplicatorImpl() { stream_.AcquirePublisher(std::move(publisher_)); }

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
  T_STREAM& stream_;
  std::unique_ptr<publisher_t> publisher_;
};

template <typename T_STREAM>
using StreamReplicator =
    current::ss::StreamSubscriber<StreamReplicatorImpl<T_STREAM>, typename T_STREAM::entry_t>;

}  // namespace sherlock
}  // namespace current

#endif  // REMOTE_STREAM_REPLICATOR_H
