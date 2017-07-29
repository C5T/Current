/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2016 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef CURRENT_STREAM_STREAM_IMPL_H
#define CURRENT_STREAM_STREAM_IMPL_H

#include "../port.h"

#include <map>
#include <thread>

#include "../bricks/util/random.h"
#include "../bricks/util/waitable_terminate_signal.h"

#include "../blocks/persistence/memory.h"
#include "../blocks/persistence/file.h"
#include "../blocks/ss/pubsub.h"

namespace current {
namespace stream {

constexpr static const char* kStreamHeaderCurrentStreamSize = "X-Current-Stream-Size";
constexpr static const char* kStreamHeaderCurrentSubscriptionId = "X-Current-Stream-Subscription-Id";

// A generic top-level `SubscriberScope` to unite any implementations, to allow `std::move()`-ing them into one.
// Features:
// 1) Any per-stream and per-type `SyncSubscriber` can be moved into this "global" `SubscriberScope`.
// 2) A `nullptr` can be assigned to this "global" `SubscriberScope`.
// 3) Unlike per-{stream/type} subscriber scopes, the "global" `SubscriberScope` can be default-constructed.
class SubscriberScope {
 public:
  class SubscriberThread {
   public:
    SubscriberThread() : subscriber_thread_done_(false) {}
    virtual ~SubscriberThread() = default;
    bool IsSubscriberThreadDone() const { return subscriber_thread_done_; }

   protected:
    std::atomic_bool subscriber_thread_done_;
  };

  SubscriberScope() = default;
  SubscriberScope(std::unique_ptr<SubscriberThread>&& thread) : thread_(std::move(thread)) {}
  SubscriberScope(SubscriberScope&& rhs) {
    std::lock_guard<std::mutex> rhs_lock(rhs.mutex_);
    thread_ = std::move(rhs.thread_);
  }
  SubscriberScope& operator=(SubscriberScope&& rhs) {
    std::lock(mutex_, rhs.mutex_);
    std::lock_guard<std::mutex> lk1(mutex_, std::adopt_lock);
    std::lock_guard<std::mutex> lk2(rhs.mutex_, std::adopt_lock);
    thread_ = std::move(rhs.thread_);
    return *this;
  }

  SubscriberScope& operator=(std::nullptr_t) {
    std::lock_guard<std::mutex> lock(mutex_);
    thread_ = nullptr;
    return *this;
  }

  operator bool() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return thread_ && !thread_->IsSubscriberThreadDone();
  }

  SubscriberScope(const SubscriberScope&) = delete;
  SubscriberScope& operator=(const SubscriberScope&) = delete;

 private:
  mutable std::mutex mutex_;
  std::unique_ptr<SubscriberThread> thread_;
};

// For asynchronous HTTP subscriptions to be terminatable, they are stored as `std::unique_ptr`-s,
// which does need an abstract base.
class AbstractSubscriberObject {
 public:
  virtual ~AbstractSubscriberObject() = default;
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER>
struct StreamImpl {
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<entry_t>;

  // Publishing-related mutex and notifier are mutable to wait on them from the subscriber thread.
  mutable std::mutex publishing_mutex;
  persistence_layer_t persister;
  mutable current::WaitableTerminateSignalBulkNotifier notifier;

  // The HTTP-subscription-related logic is `mutable` because subscribing to a stream is `const` by convention.
  using http_subscriptions_t =
      std::unordered_map<std::string, std::pair<SubscriberScope, std::unique_ptr<AbstractSubscriberObject>>>;
  mutable std::mutex http_subscriptions_mutex;
  mutable http_subscriptions_t http_subscriptions;

  template <typename... ARGS>
  StreamImpl(ARGS&&... args)
      : persister(publishing_mutex, std::forward<ARGS>(args)...) {}
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER>
class StreamPublisherImpl {
 public:
  using data_t = StreamImpl<ENTRY, PERSISTENCE_LAYER>;

  StreamPublisherImpl() = delete;
  explicit StreamPublisherImpl(Borrowed<data_t> data) : data_(std::move(data)) {}

  operator bool() const { return data_; }

  template <current::locks::MutexLockStatus MLS,
            typename E,
            typename TIMESTAMP,
            class = ENABLE_IF<ss::CanPublish<current::decay<E>, ENTRY>::value>>
  idxts_t PublisherPublishImpl(E&& e, TIMESTAMP&& timestamp) {
    const auto result =
        data_->persister.template PersisterPublishImpl<MLS>(std::forward<E>(e), std::forward<TIMESTAMP>(timestamp));
    data_->notifier.NotifyAllOfExternalWaitableEvent();
    return result;
  }

  template <current::locks::MutexLockStatus MLS>
  idxts_t PublisherPublishUnsafeImpl(const std::string& entry_json) {
    const auto result = data_->persister.template PersisterPublishUnsafeImpl<MLS>(entry_json);
    data_->notifier.NotifyAllOfExternalWaitableEvent();
    return result;
  }

  template <current::locks::MutexLockStatus MLS, typename TIMESTAMP>
  void PublisherUpdateHeadImpl(TIMESTAMP&& timestamp) {
    data_->persister.template PersisterUpdateHeadImpl<MLS>(std::forward<TIMESTAMP>(timestamp));
    data_->notifier.NotifyAllOfExternalWaitableEvent();
  }

 private:
  Borrowed<data_t> data_;
};

}  // namespace stream
}  // namespace current

#endif  // CURRENT_STREAM_STREAM_IMPL_H
