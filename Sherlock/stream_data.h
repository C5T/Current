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

#ifndef CURRENT_SHERLOCK_STREAM_DATA_H
#define CURRENT_SHERLOCK_STREAM_DATA_H

#include "../port.h"

#include <map>
#include <thread>

#include "../Blocks/Persistence/persistence.h"
#include "../Bricks/util/random.h"
#include "../Bricks/util/sha256.h"
#include "../Bricks/util/waitable_terminate_signal.h"

namespace current {
namespace sherlock {

constexpr static const char* kSherlockHeaderCurrentStreamSize = "X-Current-Stream-Size";
constexpr static const char* kSherlockHeaderCurrentSubscriptionId = "X-Current-Stream-Subscription-Id";

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

class AbstractSubscriberObject {
 public:
  virtual ~AbstractSubscriberObject() = default;
};

template <typename ENTRY, template <typename> class PERSISTENCE_LAYER>
struct StreamData {
  using entry_t = ENTRY;
  using persistence_layer_t = PERSISTENCE_LAYER<entry_t>;

  using http_subscriptions_t =
      std::unordered_map<std::string, std::pair<SubscriberScope, std::unique_ptr<AbstractSubscriberObject>>>;
  persistence_layer_t persistence;
  current::WaitableTerminateSignalBulkNotifier notifier;
  std::mutex publish_mutex;

  http_subscriptions_t http_subscriptions;
  std::mutex http_subscriptions_mutex;

  template <typename... ARGS>
  StreamData(ARGS&&... args)
      : persistence(std::forward<ARGS>(args)...) {}

  static std::string GenerateRandomHTTPSubscriptionID() {
    return current::SHA256("sherlock_http_subscription_" +
                           current::ToString(current::random::CSRandomUInt64(0ull, ~0ull)));
  }
};

}  // namespace sherlock
}  // namespace current

#endif  // CURRENT_SHERLOCK_STREAM_DATA_H
