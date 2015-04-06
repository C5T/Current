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

#ifndef SHERLOCK_KV_STORAGE_H
#define SHERLOCK_KV_STORAGE_H

#include <atomic>
#include <future>
#include <string>

#include "sherlock.h"

#include "../Bricks/mq/inmemory/mq.h"

namespace sherlock {
namespace kv_storage {

// TODO(dkorolev): Exception hierarchy for KeyValueStorage.
struct KeyNotPresentException : bricks::Exception {};

// API calls, `[Async]{Get/Set}(...)`.
//
// C++ API supports three syntaxes:
//
//   1) Asynchronous, using std::{promise/future}.
//      To use the async/await paradigm.
//      `AsyncSet(key, value)` and `AsyncGet(key)`, both return a future. The `Get()` one may throw.
//
//   2) Asynchronous, using std::function as a callback.
//      Best for HTTP endpoints, with the Request object passed into this callback.
//      `AsyncSet(key, value, callback)` and `AsyncGet(key, callback)`, both return void.
//
//   3) Synchronous. `Get(key)` and `Set(key, value)`. The `Get()` one may throw.

// TODO(dkorolev): Should deletion be a separate method?

// TODO(dkorolev): Do we allow overwrites? Or throw as well?

// TODO(dkorolev): Polymorphic types. Pass them in as std::tuple<...>, or directly as a variadic template param.

// TODO(dkorolev): How about HTTP endpoints? They should be added somewhat automatically, right?
//                 And it would require those `Scoped*` endpoints in Bricks. On me.

// TODO(dkorolev): How to handle deletion in binary serialization format?
//                 JSON can be empty, which is non-ambiguous, while in case of binary we need to differentiate
//                 between key not present or key present with empty value.

// TODO(dkorolev): So, are we CRUD+REST or just a KeyValueStorage? Now we kinda merged POST+PUT. Not cool.

template <typename T_ENTRY>
class API {
 public:
  API(const std::string& stream_name)
      : stream_(sherlock::Stream<T_ENTRY>(stream_name)), listener_scope_(stream_.Subscribe(listener_)) {}

  typename sherlock::StreamInstance<T_ENTRY>& UnsafeStream() { return stream_; }

  template <typename F>
  typename StreamInstanceImpl<T_ENTRY>::template ListenerScope<F> Subscribe(F& listener) {
    return std::move(stream_.Subscribe(listener));
  }

  std::future<typename T_ENTRY::Value> AsyncGet(typename T_ENTRY::Key&& key) {
    std::promise<typename T_ENTRY::Value> pr;
    std::future<typename T_ENTRY::Value> future = pr.get_future();
    listener_.mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  typename T_ENTRY::Value Get(typename T_ENTRY::Key&& key) {
    return AsyncGet(std::forward<typename T_ENTRY::Key>(key)).get();
  }

  std::future<void> AsyncSet(const T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();
    // The order of the next two calls is important for eventual consistency.
    listener_.mq_.EmplaceMessage(new MQMessageSet(entry, std::move(pr)));
    stream_.Publish(entry);
    return future;
  }

  void Set(const T_ENTRY& entry) { AsyncSet(entry).wait(); }

  // For testing purposes.
  bool CaughtUp() const { return listener_.caught_up_; }
  size_t EntriesSeen() const { return listener_.entries_seen_; }

 private:
  // Stateful storage.
  struct Storage {
    std::unordered_map<typename T_ENTRY::Key, typename T_ENTRY::Value, typename T_ENTRY::Key::HashFunction>
        data;
  };

  // The logic to "interleave" updates from Sherlock stream with inbound KeyValueStorage requests.
  struct MQMessage {
    virtual void DoIt(Storage& storage) = 0;
  };

  struct MQMessageEntry : MQMessage {
    // TODO(dkorolev): A single entry is fine to copy, polymorphic ones should be std::move()-d.
    T_ENTRY entry;

    explicit MQMessageEntry(const T_ENTRY& entry) : entry(entry) {}

    virtual void DoIt(Storage& storage) { storage.data[entry.key] = entry.value; }
  };

  struct MQMessageGet : MQMessage {
    typename T_ENTRY::Key key;
    std::promise<typename T_ENTRY::Value> pr;

    explicit MQMessageGet(const typename T_ENTRY::Key& key, std::promise<typename T_ENTRY::Value>&& pr)
        : key(key), pr(std::move(pr)) {}

    virtual void DoIt(Storage& storage) {
      const auto cit = storage.data.find(key);
      if (cit != storage.data.end()) {
        pr.set_value(cit->second);
      } else {
        pr.set_exception(std::make_exception_ptr(KeyNotPresentException()));
      }
    }
  };

  struct MQMessageSet : MQMessage {
    T_ENTRY e;
    std::promise<void> pr;

    explicit MQMessageSet(const T_ENTRY& e, std::promise<void>&& pr) : e(e), pr(std::move(pr)) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageSet::DoIt()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Set` pushes the `MQMessageSet` message before publishing
    // an update to the stream, the final state will always be [evenually] consistent.
    // The practical implication here is that an API `Get()` after an api `Set()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void DoIt(Storage& storage) {
      // TODO(dkorolev): Something smarter than a plain overwrite? Or make it part of the policy?
      storage.data[e.key] = e.value;
      pr.set_value();
    }
  };

  // TODO(dkorolev): In this design I have combined Sherlock listener and MMQ consumer into a single class.
  // This is probably not what the production usecase should be, especially
  // if we plan on converging MMQ's and Listener's client API calls. Need to chat with Max.
  // On the other hand, if we're not merging Sherlock and MMQ yet, we might well be good to go.

  struct StorageStateMaintainer {
    StorageStateMaintainer() : caught_up_(false), entries_seen_(0u), mq_(*this) {}

    // Sherlock stream listener call.
    bool Entry(T_ENTRY& entry, size_t index, size_t total) {
      // The logic of this API implementation is:
      // * Defer all API requests until the persistent part of the stream is fully replayed,
      // * Allow all API requests after that.

      // TODO(dkorolev): If that's the way to go, we should probably respond with HTTP 503 or 409 or 418?
      //                 (And add a `/statusz` endpoint to monitor the status wrt ready / not yet ready.)
      // TODO(dkorolev): What about an empty stream? :-)

      if (index + 1 == total) {
        caught_up_ = true;
      }

      // Non-polymorphic usecase.
      // TODO(dkorolev): Eliminate the copy && code up the polymorphic scenario for this call. In another class.
      mq_.EmplaceMessage(new MQMessageEntry(entry));

      // This is primarily for unit testing purposes.
      ++entries_seen_;

      return true;
    }

    // Sherlock stream listener call.
    void Terminate() {
      // TODO(dkorolev): Should stop serving API requests.
      // TODO(dkorolev): Should un-register HTTP endpoints, if they have been registered.
    }

    // MMQ consumer call.
    void OnMessage(std::unique_ptr<MQMessage>& message, size_t dropped_count) {
      // TODO(dkorolev): Should use a non-dropping MMQ here, of course.
      static_cast<void>(dropped_count);  // TODO(dkorolev): And change the method's signature to remove this.
      message->DoIt(storage_);
    }

    std::atomic_bool caught_up_;
    std::atomic_size_t entries_seen_;
    Storage storage_;
    MMQ<StorageStateMaintainer, std::unique_ptr<MQMessage>> mq_;
  };

  API() = delete;

  typename sherlock::StreamInstance<T_ENTRY> stream_;
  StorageStateMaintainer listener_;
  typename sherlock::StreamInstance<T_ENTRY>::template ListenerScope<StorageStateMaintainer> listener_scope_;
};

}  // namespace kv_storage
}  // namespace sherlock

#endif  // SHERLOCK_KV_STORAGE_H
