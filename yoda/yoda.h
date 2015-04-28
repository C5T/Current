/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

// Yoda is a simple to use, machine-learning-friendly key-value storage atop a Sherlock stream.
//
// The C++ usecase is to instantiate `Yoda<MyEntryType> API("stream_name")`.
// The API supports CRUD-like operations, presently `Get` and `Add`.
//
// The API offers three ways to invoke itself:
//
//   1) Asynchronous, using std::{promise/future}, that utilize the the async/await paradigm.
//      For instance, `AsyncAdd(entry)` and `AsyncGet(key)`, which both return a future.
//
//   2) Asynchronous, using std::function as callbacks. Best for HTTP endpoints and other thread-based usecases.
//      For instance, `AsyncAdd(entry, on_success, on_failure)` and `AsyncGet(key, on_success, on_failure)`,
//      which both return `void` immediately and invoke the callback as the result is ready, from a separate
//      thread.
//
//   3) Synchronous, blocking calls.
//      For instance, `Get(key)` and `Add(entry)`.
//
// Exceptions strategy:
//
//   `Async*()` versions of the callers that take callbacks and return immediately never throw.
//
//   For synchronous and promise-based implementations:
//
//   1) `Get()` and `AsyncGet()` throw the `KeyNotFoundException` if the key has not been found.
//
//      This behavior can be overriden by inheriting the entry type from `yoda::Nullable`, and adding:
//
//        constexpr static bool allow_nonthrowing_get = true;
//
//      to the definition of the entry class.
//
//   2) `Add()` and `AsyncGet()` throw `KeyAlreadyExistsException` if an entry with this key already exists.
//
//      This behavior can be overriden by inheriting the entry type from `yoda::AllowOverwriteOnAdd` or adding:
//
//        constexpr static bool allow_overwrite_on_add = true;
//
//      to the definition of the entry class.
//
// Policies:
//
//   Yoda's behavior can be customized further by providing a POLICY as a second template parameter to
//   yoda::API.
//   This struct should contain all the members described in the "Exceptions" section above.
//   Please refer to `struct DefaultPolicy` below for more details.
//
// TODO(dkorolev): Polymorphic types. Pass them in as std::tuple<...>, or directly as a variadic template param.

// TODO(dkorolev): How about HTTP endpoints? They should be added somewhat automatically, right?
//                 And it would require those `Scoped*` endpoints in Bricks. On me.

// TODO(dkorolev): So, are we CRUD+REST or just a KeyValueStorage? Now we kinda merged POST+PUT. Not cool.

#ifndef SHERLOCK_YODA_YODA_H
#define SHERLOCK_YODA_YODA_H

#include <atomic>
#include <future>
#include <string>
#include <tuple>

#include "types.h"
#include "policy.h"
#include "exceptions.h"

#include "../sherlock.h"

#include "../../Bricks/variadic/variadic.h"
#include "../../Bricks/mq/inmemory/mq.h"

namespace yoda {

template <typename>
struct Container {};

template <typename ENTRY>
struct Container<KeyEntry<ENTRY>> {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  T_MAP_TYPE<T_KEY, T_ENTRY> data;
};

// The logic to "interleave" updates from Sherlock stream with inbound KeyEntryStorage requests.
template <typename>
struct MQMessage {};

template <typename ENTRY>
struct MQMessage<KeyEntry<ENTRY>> {
  typedef sherlock::StreamInstance<ENTRY> T_STREAM_TYPE;
  virtual void DoIt(Container<KeyEntry<ENTRY>>& container, T_STREAM_TYPE& stream) = 0;
};

template <typename>
struct MQListener {};

template <typename ENTRY>
struct MQListener<KeyEntry<ENTRY>> {
  typedef sherlock::StreamInstance<ENTRY> T_STREAM_TYPE;

  explicit MQListener(Container<KeyEntry<ENTRY>>& container, T_STREAM_TYPE& stream)
      : container_(container), stream_(stream) {}

  // MMQ consumer call.
  void OnMessage(std::unique_ptr<MQMessage<KeyEntry<ENTRY>>>& message, size_t dropped_count) {
    // TODO(dkorolev): Should use a non-dropping MMQ here, of course.
    static_cast<void>(dropped_count);  // TODO(dkorolev): And change the method's signature to remove this.
    message->DoIt(container_, stream_);
  }

  Container<KeyEntry<ENTRY>>& container_;
  T_STREAM_TYPE& stream_;
};

template <typename>
struct SherlockListener {};

template <typename ENTRY>
struct SherlockListener<KeyEntry<ENTRY>> {
  typedef ENTRY T_ENTRY;
  typedef MMQ<MQListener<KeyEntry<ENTRY>>, std::unique_ptr<MQMessage<KeyEntry<ENTRY>>>> T_MQ;

  explicit SherlockListener(T_MQ& mq) : caught_up_(false), entries_seen_(0u), mq_(mq) {}

  struct MQMessageEntry : MQMessage<KeyEntry<ENTRY>> {
    // TODO(dkorolev): A single entry is fine to copy, polymorphic ones should be std::move()-d.
    using typename MQMessage<KeyEntry<ENTRY>>::T_STREAM_TYPE;
    T_ENTRY entry;

    explicit MQMessageEntry(const T_ENTRY& entry) : entry(entry) {}

    virtual void DoIt(Container<KeyEntry<ENTRY>>& container, T_STREAM_TYPE&) override {
      // TODO(max+dima): Ensure that this storage update can't break
      // the actual state of the data.
      container.data[GetKey(entry)] = entry;
    }
  };

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

  std::atomic_bool caught_up_;
  std::atomic_size_t entries_seen_;

 private:
  T_MQ& mq_;
};

template <typename>
struct Storage {};

template <typename ENTRY>
struct Storage<KeyEntry<ENTRY>> {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef KeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  typedef MMQ<MQListener<KeyEntry<ENTRY>>, std::unique_ptr<MQMessage<KeyEntry<ENTRY>>>> T_MQ;

  explicit Storage(T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : MQMessage<KeyEntry<ENTRY>> {
    using typename MQMessage<KeyEntry<ENTRY>>::T_STREAM_TYPE;

    const T_KEY key;
    std::promise<T_ENTRY> pr;
    T_ENTRY_CALLBACK on_success;
    T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const T_KEY& key, std::promise<T_ENTRY>&& pr) : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}

    virtual void DoIt(Container<KeyEntry<ENTRY>>& container, T_STREAM_TYPE&) override {
      const auto cit = container.data.find(key);
      if (cit != container.data.end()) {
        // The entry has been found.
        if (on_success) {
          // Callback semantics.
          on_success(cit->second);
        } else {
          // Promise semantics.
          pr.set_value(cit->second);
        }
      } else {
        // The entry has not been found.
        if (on_failure) {
          // Callback semantics.
          on_failure(key);
        } else {
          // Promise semantics.
          SetPromiseToNullEntryOrThrow<T_KEY, T_ENTRY, T_KEY_NOT_FOUND_EXCEPTION,
                                       false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
                                       >::DoIt(key, pr);
        }
      }
    }
  };

  struct MQMessageAdd : MQMessage<KeyEntry<ENTRY>> {
    using typename MQMessage<KeyEntry<ENTRY>>::T_STREAM_TYPE;

    const T_ENTRY e;
    std::promise<void> pr;
    T_VOID_CALLBACK on_success;
    T_VOID_CALLBACK on_failure;

    explicit MQMessageAdd(const T_ENTRY& e, std::promise<void>&& pr) : e(e), pr(std::move(pr)) {}
    explicit MQMessageAdd(const T_ENTRY& e, T_VOID_CALLBACK on_success, T_VOID_CALLBACK on_failure)
        : e(e), on_success(on_success), on_failure(on_failure) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageAdd::DoIt()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Add` pushes the `MQMessageAdd` message before publishing
    // an update to the stream, the final state will always be [eventually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void DoIt(Container<KeyEntry<ENTRY>>& container, T_STREAM_TYPE& stream) override {
      const bool key_exists = static_cast<bool>(container.data.count(GetKey(e)));
      if (key_exists) {
        if (on_failure) {  // Callback function defined.
          on_failure();
        } else {  // Throw.
          pr.set_exception(std::make_exception_ptr(T_KEY_ALREADY_EXISTS_EXCEPTION(e)));
        }
      } else {
        container.data[GetKey(e)] = e;
        stream.Publish(e);
        if (on_success) {
          on_success();
        } else {
          pr.set_value();
        }
      }
    }
  };

  std::future<T_ENTRY> AsyncGet(const T_KEY& key) {
    std::promise<T_ENTRY> pr;
    std::future<T_ENTRY> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void AsyncGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure) {
    mq_.EmplaceMessage(new MQMessageGet(key, on_success, on_failure));
  }

  T_ENTRY Get(const T_KEY& key) { return AsyncGet(std::forward<const T_KEY>(key)).get(); }

  std::future<void> AsyncAdd(const T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();

    mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
    return future;
  }

  void AsyncAdd(const T_ENTRY& entry,
                T_VOID_CALLBACK on_success,
                T_VOID_CALLBACK on_failure = [](const T_KEY&) {}) {
    mq_.EmplaceMessage(new MQMessageAdd(entry, on_success, on_failure));
  }

  void Add(const T_ENTRY& entry) { AsyncAdd(entry).get(); }

 private:
  T_MQ& mq_;
};

template <typename TYPE>
class API {};

template <typename ENTRY>
class API<std::tuple<KeyEntry<ENTRY>>> : public Storage<KeyEntry<ENTRY>> {
 public:
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef sherlock::StreamInstance<T_ENTRY> T_STREAM_TYPE;
  typedef MMQ<MQListener<KeyEntry<ENTRY>>, std::unique_ptr<MQMessage<KeyEntry<ENTRY>>>> T_MQ;

  template <typename F>
  using T_STREAM_LISTENER_TYPE = typename sherlock::StreamInstanceImpl<T_ENTRY>::template ListenerScope<F>;

  API(const std::string& stream_name)
      : Storage<KeyEntry<ENTRY>>(mq_),
        stream_(sherlock::Stream<T_ENTRY>(stream_name)),
        mq_listener_(container_, stream_),
        mq_(mq_listener_),
        sherlock_listener_(mq_),
        listener_scope_(stream_.Subscribe(sherlock_listener_)) {}

  T_STREAM_TYPE& UnsafeStream() { return stream_; }

  template <typename F>
  T_STREAM_LISTENER_TYPE<F> Subscribe(F& listener) {
    return std::move(stream_.Subscribe(listener));
  }

  // For testing purposes.
  bool CaughtUp() const { return sherlock_listener_.caught_up_; }
  size_t EntriesSeen() const { return sherlock_listener_.entries_seen_; }

 private:
  API() = delete;

  T_STREAM_TYPE stream_;
  Container<KeyEntry<ENTRY>> container_;
  MQListener<KeyEntry<ENTRY>> mq_listener_;
  T_MQ mq_;
  SherlockListener<KeyEntry<ENTRY>> sherlock_listener_;
  T_STREAM_LISTENER_TYPE<SherlockListener<KeyEntry<ENTRY>>> listener_scope_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_YODA_H
