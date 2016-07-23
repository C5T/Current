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
namespace storage {

template <typename T_STREAM>
class SubscribableRemoteStream final {
 public:
  struct RemoteStream final {
    RemoteStream(const std::string& url) : url_(url) {}

    const std::string url_;
  };

  template <typename T>
  class RemoteSubscriberScope final {
   public:
    using publisher_t = typename T::publisher_t;
    using entry_t = typename T::entry_t;

    explicit RemoteSubscriberScope(ScopeOwned<RemoteStream>& remote_stream)
        : remote_stream_(remote_stream,
                         [this]() {
                           if (thread_) {
                             TerminateSubscription();
                             thread_->join();
                           }
                         }),
          index_(0),
          stop_(false) {}

    RemoteSubscriberScope(RemoteSubscriberScope&& rhs)
        : remote_stream_(std::move(rhs.remote_stream_)), index_(rhs.index_), stop_(bool(rhs.stop_)) {}

    ~RemoteSubscriberScope() {
      if (thread_) {
        TerminateSubscription();
        thread_->join();
      }
    }

    void Subscribe() { Subscribe(index_); }

    void Subscribe(uint64_t index) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (!publisher_) CURRENT_THROW(PublisherDoesNotExistException());
      if (thread_) CURRENT_THROW(SubscriptionIsActiveException());
      index_ = index;
      thread_.reset(new std::thread([this]() { Thread(); }));
    }

    void Unsubscribe() { Unsubscribe(index_); }

    void Unsubscribe(uint64_t index) {
      if (!thread_) CURRENT_THROW(SubscriptionIsNotActiveException());
      Wait(index);
      TerminateSubscription();
      thread_->join();
      thread_.reset();
    }

    void Wait(uint64_t index) const {
      std::unique_lock<std::mutex> lock(mutex_);
      event_.wait(lock, [this, index]() { return index_ >= index; });
    }

    void AcceptPublisher(std::unique_ptr<publisher_t> publisher) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (publisher_) CURRENT_THROW(PublisherAlreadyOwnedException());
      publisher_ = std::move(publisher);
    }

    void ReturnPublisherToStream(T& stream) {
      std::unique_lock<std::mutex> lock(mutex_);
      if (!publisher_) CURRENT_THROW(PublisherAlreadyReleasedException());
      if (thread_) CURRENT_THROW(PublisherIsUsedException());
      stream.AcquirePublisher(std::move(publisher_));
    }

   private:
    void TerminateSubscription() {
      stop_ = true;
      std::unique_lock<std::mutex> lock(mutex_);
      if (!subscription_id_.empty()) {
        const std::string terminate_url = remote_stream_->url_ + "?terminate=" + subscription_id_;
        try {
          const auto result = HTTP(GET(terminate_url));
        } catch (current::net::NetworkException&) {
        }
      }
    }

    void Thread() {
      while (!stop_) {
        const std::string url = remote_stream_->url_ + "?i=" + current::ToString(index_);
        try {
          HTTP(ChunkedGET(
              url,
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
      }
    }

    void OnChunk(const std::string& chunk) {
      if (stop_) {
        return;
      }
      std::unique_lock<std::mutex> lock(mutex_);

      const auto split = current::strings::Split(chunk, '\t');
      assert(split.size() == 2u);
      const auto idx = ParseJSON<idxts_t>(split[0]);
      assert(idx.index == index_);
      auto transaction = ParseJSON<entry_t>(split[1]);
      publisher_->Publish(std::move(transaction), idx.us);
      ++index_;
      event_.notify_one();
    }

   private:
    ScopeOwnedBySomeoneElse<RemoteStream> remote_stream_;
    uint64_t index_;
    std::string subscription_id_;
    std::unique_ptr<std::thread> thread_;
    mutable std::mutex mutex_;
    mutable std::condition_variable event_;
    std::atomic_bool stop_;
    std::unique_ptr<publisher_t> publisher_;
  };

  using entry_t = typename T_STREAM::entry_t;
  using publisher_t = typename T_STREAM::publisher_t;
  using subscriber_t = RemoteSubscriberScope<T_STREAM>;

  explicit SubscribableRemoteStream(const std::string& remote_stream_url)
      : stream_(remote_stream_url),
        schema_(Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<entry_t>()).type_id,
                reflection::CurrentTypeName<entry_t>(),
                sherlock::constants::kDefaultTopLevelName,
                sherlock::constants::kDefaultNamespaceName) {
    CheckRemoteSchema();
  }

  explicit SubscribableRemoteStream(const std::string& remote_stream_url,
                                    const std::string& top_level_name,
                                    const std::string& namespace_name)
      : stream_(remote_stream_url),
        schema_(Value<reflection::ReflectedTypeBase>(reflection::Reflector().ReflectType<entry_t>()).type_id,
                reflection::CurrentTypeName<entry_t>(),
                top_level_name,
                namespace_name) {
    CheckRemoteSchema();
  }
  ~SubscribableRemoteStream(){};

  subscriber_t Subscribe(std::unique_ptr<publisher_t> publisher, uint64_t index = 0u) {
    subscriber_t subscriber(stream_);
    subscriber.AcceptPublisher(std::move(publisher));
    subscriber.Subscribe(index);
    return subscriber;
  }

  subscriber_t Subscribe(T_STREAM& local_stream, uint64_t index = 0u) {
    subscriber_t subscriber(stream_);
    local_stream.MovePublisherTo(subscriber);
    subscriber.Subscribe(index);
    return subscriber;
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

}  // namespace storage
}  // namespace current

#endif  // REMOTE_STREAM_REPLICATOR_H
