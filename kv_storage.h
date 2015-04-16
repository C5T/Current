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
struct GetNullEntry {
  static T_ENTRY get() {
    static_assert(true, "Should never happen.");
    return T_ENTRY();
  }
};

template <typename T_ENTRY>
struct GetNullEntry<T_ENTRY, true> {
  static T_ENTRY get() {
    T_ENTRY e(NullEntry);
    return e;
  }
};

template <typename T_ENTRY, bool IS_NULLABLE>
T_ENTRY CreateNullEntry() { return GetNullEntry<T_ENTRY, IS_NULLABLE>::get(); }

// Exceptions.
struct KeyNotFoundException : bricks::Exception {};

template <typename T_ENTRY>
struct SpecificEntryTypeKeyNotFoundException : KeyNotFoundException {
  typedef typename std::remove_reference<decltype(std::declval<T_ENTRY>().key())>::type T_KEY;
  T_KEY key;
  SpecificEntryTypeKeyNotFoundException(const T_KEY& key) : key(key) {}
};

struct KeyAlreadyExistsException : bricks::Exception {};

template <typename T_ENTRY>
struct SpecificEntryKeyAlreadyExistsException : KeyAlreadyExistsException {
  T_ENTRY entry;
  SpecificEntryKeyAlreadyExistsException(const T_ENTRY& entry): entry(entry) {}
};

struct EntryShouldExistException : bricks::Exception {};

template <typename T_ENTRY>
struct SpecificEntryShouldExistException : EntryShouldExistException {
  T_ENTRY entry;
  SpecificEntryShouldExistException(const T_ENTRY& entry): entry(entry) {}
};

// API calls, `[Async]{Get/Add/Update/Delete}(...)`.
//
// C++ API supports three syntaxes:
//
//   1) Asynchronous, using std::{promise/future}.
//      To use the async/await paradigm.
//      `AsyncAdd(entry)` and `AsyncGet(key)`, both return a future. The `AsyncGet()` one may throw.
//
//   2) Asynchronous, using std::function as a callback.
//      Best for HTTP endpoints, with the Request object passed into this callback.
//      `AsyncAdd(entry, on_done, on_exists)` and `AsyncGet(key, on_found, on_not_found)`, both return void.
//
//   3) Synchronous. `Get(key)` and `Add(entry)`. The `Get()` one may throw.

// TODO(dkorolev): Polymorphic types. Pass them in as std::tuple<...>, or directly as a variadic template param.

// TODO(dkorolev): How about HTTP endpoints? They should be added somewhat automatically, right?
//                 And it would require those `Scoped*` endpoints in Bricks. On me.

// TODO(dkorolev): So, are we CRUD+REST or just a KeyValueStorage? Now we kinda merged POST+PUT. Not cool.

// Policies.
template <typename ENTRY>
constexpr auto has_nonthrowing_get(int) -> decltype(std::declval<ENTRY>().allow_nonthrowing_get, bool()) {
  return true;
}

template <typename ENTRY>
constexpr bool has_nonthrowing_get(...) {
  return false;
}

template <typename ENTRY, bool>
struct ExtractNonThrowingGet {
 constexpr static bool value = ENTRY::allow_nonthrowing_get;
 static_assert(!value || std::is_base_of<Nullable, ENTRY>::value, "Entry type must be derived from Nullable.");
};

template <typename ENTRY>
struct ExtractNonThrowingGet<ENTRY, false> {
  constexpr static bool value = false;  // Default: throw if key not found.
};

template <typename ENTRY>
constexpr auto has_overwrite_on_add(int) -> decltype(std::declval<ENTRY>().allow_overwrite_on_add, bool()) {
  return true;
}

template <typename ENTRY>
constexpr bool has_overwrite_on_add(...) {
  return false;
}

template <typename ENTRY, bool>
struct ExtractOverwriteOnAdd {
 constexpr static bool value = ENTRY::allow_overwrite_on_add;
 static_assert(!value || std::is_base_of<Nullable, ENTRY>::value, "Entry type must be derived from Nullable.");
};

template <typename ENTRY>
struct ExtractOverwriteOnAdd<ENTRY, false> {
  constexpr static bool value = false;  // Default: don't overwrite on Add() if key already exists.
};

template <typename ENTRY>
struct DefaultPolicy {
  constexpr static bool allow_nonthrowing_get = ExtractNonThrowingGet<ENTRY, has_nonthrowing_get<ENTRY>(0)>::value;
  constexpr static bool allow_overwrite_on_add = ExtractOverwriteOnAdd<ENTRY, has_overwrite_on_add<ENTRY>(0)>::value;
};

// Main storage class.
template <typename ENTRY, typename POLICY = DefaultPolicy<ENTRY>>
class API {
 public:
  typedef ENTRY T_ENTRY;
  typedef typename std::remove_reference<decltype(std::declval<T_ENTRY>().key())>::type T_KEY;
  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;
  typedef POLICY T_POLICY;
  typedef SpecificEntryTypeKeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef SpecificEntryKeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef SpecificEntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  API(const std::string& stream_name)
      : stream_(sherlock::Stream<T_ENTRY>(stream_name)), listener_scope_(stream_.Subscribe(listener_)) {}

  typename sherlock::StreamInstance<T_ENTRY>& UnsafeStream() { return stream_; }

  template <typename F>
  typename StreamInstanceImpl<T_ENTRY>::template ListenerScope<F> Subscribe(F& listener) {
    return std::move(stream_.Subscribe(listener));
  }

  std::future<T_ENTRY> AsyncGet(const T_KEY&& key) {
    std::promise<T_ENTRY> pr;
    std::future<T_ENTRY> future = pr.get_future();
    listener_.mq_.EmplaceMessage(new MQMessageGet(key, std::move(pr)));
    return future;
  }

  void AsyncGet(const T_KEY&& key, T_ENTRY_CALLBACK on_found, T_KEY_CALLBACK on_not_found) {
    listener_.mq_.EmplaceMessage(new MQMessageGet(key, on_found, on_not_found));
  }

  T_ENTRY Get(const T_KEY&& key) {
    return AsyncGet(std::forward<const T_KEY>(key)).get();
  }

  std::future<void> AsyncAdd(const T_ENTRY& entry) {
    std::promise<void> pr;
    std::future<void> future = pr.get_future();
    if (listener_.storage_.data.count(entry.key()) && !T_POLICY::allow_overwrite_on_add) {
      pr.set_exception(std::make_exception_ptr(SpecificEntryKeyAlreadyExistsException<T_ENTRY>(entry)));
    } else {
      // The order of the next two calls is important for eventual consistency.
      listener_.mq_.EmplaceMessage(new MQMessageAdd(entry, std::move(pr)));
      stream_.Publish(entry);
    }
    return future;
  }

  void AsyncAdd(const T_ENTRY& entry, T_VOID_CALLBACK on_done, T_VOID_CALLBACK on_exists) {
    if (!listener_.storage_.data.count(entry.key())) {
      // The order of the next two calls is important for eventual consistency.
      listener_.mq_.EmplaceMessage(new MQMessageAdd(entry, on_done));
      stream_.Publish(entry);
    } else {
      if (T_POLICY::allow_overwrite_on_add) {
       listener_.mq_.EmplaceMessage(new MQMessageAdd(entry, on_exists));
       stream_.Publish(entry);
      } else {
        std::async(on_exists);
      }
    }
  }

  void Add(const T_ENTRY& entry) { AsyncAdd(entry).get(); }

  // For testing purposes.
  bool CaughtUp() const { return listener_.caught_up_; }
  size_t EntriesSeen() const { return listener_.entries_seen_; }

 private:
  // Stateful storage.
  struct Storage {
    std::unordered_map<T_KEY, T_ENTRY, typename T_KEY::HashFunction> data;
  };

  // The logic to "interleave" updates from Sherlock stream with inbound KeyValueStorage requests.
  struct MQMessage {
    virtual void DoIt(Storage& storage) = 0;
  };

  struct MQMessageEntry : MQMessage {
    // TODO(dkorolev): A single entry is fine to copy, polymorphic ones should be std::move()-d.
    T_ENTRY entry;

    explicit MQMessageEntry(const T_ENTRY& entry) : entry(entry) {}

    virtual void DoIt(Storage& storage) { storage.data[entry.key()] = entry; }
  };

  struct MQMessageGet : MQMessage {
    T_KEY key;
    std::promise<T_ENTRY> pr;
    T_ENTRY_CALLBACK on_found;
    T_KEY_CALLBACK on_not_found;

    explicit MQMessageGet(const T_KEY& key, std::promise<T_ENTRY>&& pr)
        : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const T_KEY& key, T_ENTRY_CALLBACK on_found, T_KEY_CALLBACK on_not_found)
        : key(key), on_found(on_found), on_not_found(on_not_found) {}

    virtual void DoIt(Storage& storage) {
      const auto cit = storage.data.find(key);
      if (cit != storage.data.end()) {
        if (on_found) {  // Callback function defined.
          std::async(on_found, cit->second);
        } else {
          pr.set_value(cit->second);
        }
      } else {  // Key not found.
       if (on_not_found) {  // Callback function defined.
          std::async(on_not_found, key);
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
    T_ENTRY e;
    std::promise<void> pr;
    T_VOID_CALLBACK callback;

    explicit MQMessageAdd(const T_ENTRY& e, std::promise<void>&& pr)
        : e(e), pr(std::move(pr)) {}
    explicit MQMessageAdd(const T_ENTRY& e, T_VOID_CALLBACK callback)
        : e(e), callback(callback) {}

    // Important note: The entry added will eventually reach the storage via the stream.
    // Thus, in theory, `MQMessageAdd::DoIt()` could be a no-op.
    // This code still updates the storage, to have the API appear more lively to the user.
    // Since the actual implementation of `Add` pushes the `MQMessageAdd` message before publishing
    // an update to the stream, the final state will always be [evenually] consistent.
    // The practical implication here is that an API `Get()` after an api `Add()` may and will return data,
    // that might not yet have reached the storage, and thus relying on the fact that an API `Get()` call
    // reflects updated data is not reliable from the point of data synchronization.
    virtual void DoIt(Storage& storage) {
      storage.data[e.key()] = e;
      if (callback) {
        std::async(callback);
      } else {
        pr.set_value();
      }
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
