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

#ifndef SHERLOCK_YODA_YODA_H
#define SHERLOCK_YODA_YODA_H

#include <atomic>
#include <future>
#include <string>

#include "../sherlock.h"

#include "../../Bricks/mq/inmemory/mq.h"

namespace sherlock {
namespace yoda {

// Pure key type alias.
template <typename T_ENTRY>
using ENTRY_KEY_TYPE = typename std::remove_cv<
    typename std::remove_reference<decltype(std::declval<T_ENTRY>().key())>::type>::type;

// Exceptions.
struct NonThrowingGetPrerequisitesNotMetException : bricks::Exception {
  explicit NonThrowingGetPrerequisitesNotMetException(const std::string& what) { SetWhat(what); }
};

struct KeyNotFoundCoverException : bricks::Exception {};

template <typename T_ENTRY>
struct KeyNotFoundException : KeyNotFoundCoverException {
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;
  const T_KEY key;
  explicit KeyNotFoundException(const T_KEY& key) : key(key) {}
};

struct KeyAlreadyExistsCoverException : bricks::Exception {};

template <typename T_ENTRY>
struct KeyAlreadyExistsException : KeyAlreadyExistsCoverException {
  const T_ENTRY entry;
  explicit KeyAlreadyExistsException(const T_ENTRY& entry) : entry(entry) {}
};

struct EntryShouldExistCoverException : bricks::Exception {};

template <typename T_ENTRY>
struct EntryShouldExistException : EntryShouldExistCoverException {
  const T_ENTRY entry;
  explicit EntryShouldExistException(const T_ENTRY& entry) : entry(entry) {}
};

// Base structures for user entry types.
struct AllowNonThrowingGet {
  constexpr static bool allow_nonthrowing_get = true;
};

enum NullEntryTypeHelper { NullEntry };
struct Nullable {
  const bool exists;
  Nullable() : exists(true) {}
  Nullable(NullEntryTypeHelper) : exists(false) {}
};

struct Deletable {};  // User promises to serialize Nullable::exists;

// Universal null entry creation.
template <typename T_ENTRY, bool IS_NULLABLE>
struct CreateNullEntryImpl {
  static T_ENTRY get() {
    // This function shoud NEVER be called.
    // If you see this exception, something went terribly wrong.
    BRICKS_THROW(NonThrowingGetPrerequisitesNotMetException(
        "Creating NullEntry requested for the entry type, which is not derived from Nullable."));
  }
};

template <typename T_ENTRY>
struct CreateNullEntryImpl<T_ENTRY, true> {
  static T_ENTRY get() {
    T_ENTRY e(NullEntry);
    return e;
  }
};

template <typename T_ENTRY, bool IS_NULLABLE>
T_ENTRY CreateNullEntry() {
  return CreateNullEntryImpl<T_ENTRY, IS_NULLABLE>::get();
}

// API calls, `[Async]{Get/Add/Update/Delete}(...)`.
//
// C++ API supports three syntaxes:
//
//   1) Asynchronous, using std::{promise/future}.
//      To use the async/await paradigm.
//      `AsyncAdd(entry)` and `AsyncGet(key)`, both return a future.
//
//   2) Asynchronous, using std::function as callbacks.
//      Best for HTTP endpoints, with the Request object passed into these callbacks.
//      `AsyncAdd(entry, on_success, on_failure)` and `AsyncGet(key, on_success, on_failure)`, both return void.
//
//   3) Synchronous. `Get(key)` and `Add(entry)`.
//
// Exceptions:
//
//   1) By default, `Get` and `AsyncGet` throw `KeyNotFoundException` if key is
//      not found. User can change this behavior by adding
//        constexpr static bool allow_nonthrowing_get = true;
//      to his entry class definition.
//
//   2) By default, `Add` and `AsyncGet` throw `KeyAlreadyExistsException` if
//      entry with this key already exists. User can change this behavior by
//      addding
//        constexpr static bool allow_overwrite_on_add = true;
//      to his entry class definition.
//
// Policies:
//
//   KVS exception-throwing behavior can be also set up by providing POLICY struct
//   as a second template parameter to yoda::API. This struct should contain all
//   the members described in the section 'Exceptions' (see `struct
//   DefaultPolicy` below for example).
//
// TODO(dkorolev): Polymorphic types. Pass them in as std::tuple<...>, or directly as a variadic template param.

// TODO(dkorolev): How about HTTP endpoints? They should be added somewhat automatically, right?
//                 And it would require those `Scoped*` endpoints in Bricks. On me.

// TODO(dkorolev): So, are we CRUD+REST or just a KeyValueStorage? Now we kinda merged POST+PUT. Not cool.

// Policies.
template <typename ENTRY>
constexpr auto nonthrowing_get_field(int) -> decltype(std::declval<ENTRY>().allow_nonthrowing_get, bool()) {
  return ENTRY::allow_nonthrowing_get;
};

template <typename ENTRY>
constexpr bool nonthrowing_get_field(...) {
  return false;  // Default: throw if key is not found.
}

template <typename ENTRY>
constexpr auto overwrite_on_add_field(int) -> decltype(std::declval<ENTRY>().allow_overwrite_on_add, bool()) {
  return ENTRY::allow_overwrite_on_add;
}

template <typename ENTRY>
constexpr bool overwrite_on_add_field(...) {
  return false;  // Default: don't overwrite on Add() if key already exists.
}

template <typename ENTRY>
struct DefaultPolicy {
  constexpr static bool allow_nonthrowing_get = nonthrowing_get_field<ENTRY>(0);
  constexpr static bool allow_overwrite_on_add = overwrite_on_add_field<ENTRY>(0);
  static_assert(!allow_nonthrowing_get || std::is_base_of<Nullable, ENTRY>::value,
                "Entry types that requested non-throwing `Get()` must be derived from Nullable.");
};

// Main storage class.
template <typename ENTRY, typename POLICY = DefaultPolicy<ENTRY>>
class API {
 public:
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<ENTRY> T_KEY;
  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;
  typedef POLICY T_POLICY;
  typedef KeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  API(const std::string& stream_name)
      : stream_(sherlock::Stream<T_ENTRY>(stream_name)),
        state_maintainer_(stream_),
        listener_scope_(stream_.Subscribe(state_maintainer_)) {}

  typename sherlock::StreamInstance<T_ENTRY>& UnsafeStream() { return stream_; }

  template <typename F>
  typename StreamInstanceImpl<T_ENTRY>::template ListenerScope<F> Subscribe(F& listener) {
    return std::move(stream_.Subscribe(listener));
  }

  std::future<T_ENTRY> AsyncGet(const T_KEY&& key) {
    std::promise<T_ENTRY> pr;
    std::future<T_ENTRY> future = pr.get_future();
    state_maintainer_.mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void AsyncGet(const T_KEY&& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure = [](const T_KEY&) {
  }) {
    state_maintainer_.mq_.EmplaceMessage(new MQMessageGet(key, on_success, on_failure));
  }

  T_ENTRY Get(const T_KEY&& key) { return AsyncGet(std::forward<const T_KEY>(key)).get(); }

  std::future<void> AsyncAdd(const T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();

    state_maintainer_.mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
    return future;
  }

  void AsyncAdd(const T_ENTRY& entry,
                T_VOID_CALLBACK on_success,
                T_VOID_CALLBACK on_failure = [](const T_KEY&) {}) {
    state_maintainer_.mq_.EmplaceMessage(new MQMessageAdd(entry, on_success, on_failure));
  }

  void Add(const T_ENTRY& entry) { AsyncAdd(entry).get(); }

  // For testing purposes.
  bool CaughtUp() const { return state_maintainer_.caught_up_; }
  size_t EntriesSeen() const { return state_maintainer_.entries_seen_; }

 private:
  // Stateful storage.
  struct Storage {
    std::unordered_map<T_KEY, T_ENTRY, typename T_KEY::HashFunction> data;
  };

  // The logic to "interleave" updates from Sherlock stream with inbound KeyValueStorage requests.
  struct MQMessage {
    virtual void DoIt(Storage& storage, typename sherlock::StreamInstance<T_ENTRY>& stream) = 0;
  };

  struct MQMessageEntry : MQMessage {
    // TODO(dkorolev): A single entry is fine to copy, polymorphic ones should be std::move()-d.
    T_ENTRY entry;

    explicit MQMessageEntry(const T_ENTRY& entry) : entry(entry) {}

    virtual void DoIt(Storage& storage, typename sherlock::StreamInstance<T_ENTRY>&) {
      // TODO(max+dima): Ensure that this storage update can't break
      // the actual state of the data.
      storage.data[entry.key()] = entry;
    }
  };

  struct MQMessageGet : MQMessage {
    const T_KEY key;
    std::promise<T_ENTRY> pr;
    T_ENTRY_CALLBACK on_success;
    T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const T_KEY& key, std::promise<T_ENTRY>&& pr) : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}

    virtual void DoIt(Storage& storage, typename sherlock::StreamInstance<T_ENTRY>&) {
      const auto cit = storage.data.find(key);
      if (cit != storage.data.end()) {
        if (on_success) {  // Callback function defined.
          on_success(cit->second);
        } else {
          pr.set_value(cit->second);
        }
      } else {             // Key not found.
        if (on_failure) {  // Callback function defined.
          on_failure(key);
        } else if (T_POLICY::allow_nonthrowing_get) {  // Return non-existing entry.
          T_ENTRY null_entry(CreateNullEntry<T_ENTRY, std::is_base_of<Nullable, T_ENTRY>::value>());
          null_entry.set_key(key);
          pr.set_value(null_entry);
        } else {  // Throw.
          pr.set_exception(std::make_exception_ptr(T_KEY_NOT_FOUND_EXCEPTION(key)));
        }
      }
    }
  };

  struct MQMessageAdd : MQMessage {
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
    // an update to the stream, the final state will always be [evenually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void DoIt(Storage& storage, typename sherlock::StreamInstance<T_ENTRY>& stream) {
      const bool key_exists = static_cast<bool>(storage.data.count(e.key()));
      if (key_exists && !T_POLICY::allow_overwrite_on_add) {
        if (on_failure) {  // Callback function defined.
          on_failure();
        } else {  // Throw.
          pr.set_exception(std::make_exception_ptr(T_KEY_ALREADY_EXISTS_EXCEPTION(e)));
        }
      } else {
        storage.data[e.key()] = e;
        stream.Publish(e);
        if (on_success) {
          on_success();
        } else {
          pr.set_value();
        }
      }
    }
  };

  // TODO(dkorolev): In this design I have combined Sherlock listener and MMQ consumer into a single class.
  // This is probably not what the production usecase should be, especially
  // if we plan on converging MMQ's and Listener's client API calls. Need to chat with Max.
  // On the other hand, if we're not merging Sherlock and MMQ yet, we might well be good to go.

  struct StorageStateMaintainer {
    explicit StorageStateMaintainer(typename sherlock::StreamInstance<T_ENTRY>& stream_)
        : stream_(stream_), caught_up_(false), entries_seen_(0u), mq_(*this) {}

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
      message->DoIt(storage_, stream_);
    }

    typename sherlock::StreamInstance<T_ENTRY>& stream_;
    std::atomic_bool caught_up_;
    std::atomic_size_t entries_seen_;
    Storage storage_;
    MMQ<StorageStateMaintainer, std::unique_ptr<MQMessage>> mq_;
  };

  API() = delete;

  typename sherlock::StreamInstance<T_ENTRY> stream_;
  StorageStateMaintainer state_maintainer_;
  typename sherlock::StreamInstance<T_ENTRY>::template ListenerScope<StorageStateMaintainer> listener_scope_;
};

}  // namespace yoda
}  // namespace sherlock

#endif  // SHERLOCK_YODA_YODA_H
