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

#include "../../Bricks/mq/inmemory/mq.h"
#include "../../Bricks/template/metaprogramming.h"

// The best way I found to have clang++ dump the actual type. -- D.K.
// Usage: static_assert(sizeof(is_same_or_compile_error<A, B>), "");
// TODO(dkorolev): Chat with Max, remove or move it into Bricks.
template <typename T1, typename T2>
struct is_same_or_compile_error {
  char c[std::is_same<T1, T2>::value ? 1 : -1];
};

namespace yoda {

// Essential types to define and make use of.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener;

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct SherlockListener;

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage;

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct PolymorphicContainer;

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct EssentialTypedefs {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");

  typedef ENTRY_BASE_TYPE T_ENTRY_BASE_TYPE;
  typedef SUPPORTED_TYPES_AS_TUPLE T_SUPPORTED_TYPES_AS_TUPLE;

  // A hack to extract the type of the (only) supported entry type.
  // TODO(dkorolev): Make this variadic.
  typedef UnderlyingEntryType<bricks::metaprogramming::rmconstref<
      decltype(std::get<0>(std::declval<SUPPORTED_TYPES_AS_TUPLE>()))>> HACK_T_ENTRY;

  // TODO(dkorolev): A *big* *BIG* note: Our `visitor` does not work with classes hierarchy now!
  // It results in illegal ambiguous virtual function resolution.
  // Long story short, `dynamic_cast<>` and no `ENTRY_BASE_TYPE` in the list of supported types.
  typedef std::tuple<HACK_T_ENTRY> T_VISITABLE_TYPES_AS_TUPLE;
  typedef bricks::metaprogramming::abstract_visitable<T_VISITABLE_TYPES_AS_TUPLE> T_ABSTRACT_VISITABLE;

  typedef MQListener<ENTRY_BASE_TYPE, T_SUPPORTED_TYPES_AS_TUPLE> T_MQ_LISTENER;
  typedef MQMessage<ENTRY_BASE_TYPE, T_SUPPORTED_TYPES_AS_TUPLE> T_MQ_MESSAGE;
  typedef MMQ<T_MQ_LISTENER, std::unique_ptr<T_MQ_MESSAGE>> T_MQ;

  typedef PolymorphicContainer<T_SUPPORTED_TYPES_AS_TUPLE> T_CONTAINER;

  typedef sherlock::StreamInstance<std::unique_ptr<T_ENTRY_BASE_TYPE>> T_STREAM_TYPE;

  template <typename F>
  using T_STREAM_LISTENER_TYPE = typename T_STREAM_TYPE::template ListenerScope<F>;

  typedef SherlockListener<ENTRY_BASE_TYPE, T_SUPPORTED_TYPES_AS_TUPLE> T_SHERLOCK_LISTENER;

  typedef T_STREAM_LISTENER_TYPE<T_SHERLOCK_LISTENER> T_SHERLOCK_LISTENER_SCOPE_TYPE;
};

// The logic to keep the in-memory state of a particular entry type and its internal represenation.
template <typename>
struct Container {};

// TODO(dkorolev): With variadic implementation, the type list for the visitor will be taken from another place.
template <typename ENTRY>
struct Container<KeyEntry<ENTRY>> : bricks::metaprogramming::visitor<std::tuple<ENTRY>> {
  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  T_MAP_TYPE<T_KEY, T_ENTRY> data;

  // The container itself is visitable by enties.
  // The entries passed in this way are the entries scanned from the Sherlock stream.
  // The default behavior is to update the contents of the container.
  virtual void visit(ENTRY& entry) override { data[GetKey(entry)] = entry; }
};

// The logic to "interleave" updates from Sherlock stream with inbound KeyEntryStorage requests.
// TODO(dkorolev): This guy should be polymorphic too.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> ET;
  virtual void DoIt(typename ET::T_CONTAINER::type& container, typename ET::T_STREAM_TYPE& stream) = 0;
};

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> ET;
  explicit MQListener(typename ET::T_CONTAINER::type& container, typename ET::T_STREAM_TYPE& stream)
      : container_(container), stream_(stream) {}

  // MMQ consumer call.
  void OnMessage(std::unique_ptr<typename ET::T_MQ_MESSAGE>& message, size_t dropped_count) {
    // TODO(dkorolev): Should use a non-dropping MMQ here, of course.
    static_cast<void>(dropped_count);  // TODO(dkorolev): And change the method's signature to remove this.
    message->DoIt(container_, stream_);
  }

  typename ET::T_CONTAINER::type& container_;
  typename ET::T_STREAM_TYPE& stream_;
};

// TODO(dkorolev): This piece of code should also be made polymorphic.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct SherlockListener {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

template <typename ENTRY_BASE_TYPE, typename ENTRY>
struct SherlockListener<ENTRY_BASE_TYPE, std::tuple<KeyEntry<ENTRY>>> {
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, std::tuple<KeyEntry<ENTRY>>> ET;

  explicit SherlockListener(typename ET::T_MQ& mq) : caught_up_(false), entries_seen_(0u), mq_(mq) {}

  struct MQMessageEntry : MQMessage<ENTRY_BASE_TYPE, typename ET::T_SUPPORTED_TYPES_AS_TUPLE> {
    std::unique_ptr<ENTRY_BASE_TYPE> entry;

    explicit MQMessageEntry(std::unique_ptr<ENTRY_BASE_TYPE>&& entry) : entry(std::move(entry)) {}

    // TODO(dkorolev): Replace this by a variadic implementation. Metaprogramming MapReduce FTW!
    static_assert(sizeof(is_same_or_compile_error<typename ET::T_VISITABLE_TYPES_AS_TUPLE, std::tuple<ENTRY>>),
                  "");

    virtual void DoIt(typename ET::T_CONTAINER::type& container, typename ET::T_STREAM_TYPE& stream) override {
      // TODO(dkorolev): Move this logic to the *RIGHT* place.
      // TODO(max+dima): Ensure that this storage update can't break the actual state of the data.

      typename ET::T_ABSTRACT_VISITABLE* av = dynamic_cast<typename ET::T_ABSTRACT_VISITABLE*>(entry.get());
      if (av) {
        av->accept(container);
      } else {
        // TODO(dkorolev): Talk to Max about API's policy on the stream containing an entry of unsupported type.
        // Ex., have base entry type B, entry types X and Y, and finding an entry of type Z in the stream.
        // Fail with an error message by default?
        throw false;
      }

      static_cast<void>(stream);
    }
  };

  // Sherlock stream listener call.
  bool Entry(std::unique_ptr<ENTRY_BASE_TYPE>& entry, size_t index, size_t total) {
    // The logic of this API implementation is:
    // * Defer all API requests until the persistent part of the stream is fully replayed,
    // * Allow all API requests after that.

    mq_.EmplaceMessage(new MQMessageEntry(std::move(entry)));

    // TODO(dkorolev): If that's the way to go, we should probably respond with HTTP 503 or 409 or 418?
    //                 (And add a `/statusz` endpoint to monitor the status wrt ready / not yet ready.)

    // TODO(dkorolev)+TODO(mzhurovich): What about the case of an empty stream?
    // We should probably extend/enable Sherlock stream listener to report the `CaughtUp` event early,
    // including the case where the initial stream is empty.

    if (index + 1 == total) {
      caught_up_ = true;
    }

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
  typename ET::T_MQ& mq_;
};

// TODO(dkorolev): Enable the variadic version.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct PolymorphicContainer {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

template <typename ENTRY>
struct PolymorphicContainer<std::tuple<KeyEntry<ENTRY>>> {
  typedef Container<KeyEntry<ENTRY>> type;
};

/*
// TODO(dkorolev): Make better use of this piece of logic.
template <typename SUPPORTED_TYPES_AS_TUPLE>
using RealPolymorphicContainer = typename PolymorphicContainer<SUPPORTED_TYPES_AS_TUPLE>::type;
*/

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct Storage {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  Storage() = delete;
};

// TODO(dkorolev): This should be polymorphic- and variadic-friendly.
template <typename ENTRY_BASE_TYPE, typename ENTRY>
struct Storage<ENTRY_BASE_TYPE, KeyEntry<ENTRY>> {
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, std::tuple<KeyEntry<ENTRY>>> ET;

  typedef ENTRY T_ENTRY;
  typedef ENTRY_KEY_TYPE<T_ENTRY> T_KEY;

  typedef std::function<void(const T_ENTRY&)> T_ENTRY_CALLBACK;
  typedef std::function<void(const T_KEY&)> T_KEY_CALLBACK;
  typedef std::function<void()> T_VOID_CALLBACK;

  typedef KeyNotFoundException<T_ENTRY> T_KEY_NOT_FOUND_EXCEPTION;
  typedef KeyAlreadyExistsException<T_ENTRY> T_KEY_ALREADY_EXISTS_EXCEPTION;
  typedef EntryShouldExistException<T_ENTRY> T_ENTRY_SHOULD_EXIST_EXCEPTION;

  Storage() = delete;
  explicit Storage(typename ET::T_MQ& mq) : mq_(mq) {}

  struct MQMessageGet : ET::T_MQ_MESSAGE {
    const T_KEY key;
    std::promise<T_ENTRY> pr;
    T_ENTRY_CALLBACK on_success;
    T_KEY_CALLBACK on_failure;

    explicit MQMessageGet(const T_KEY& key, std::promise<T_ENTRY>&& pr) : key(key), pr(std::move(pr)) {}
    explicit MQMessageGet(const T_KEY& key, T_ENTRY_CALLBACK on_success, T_KEY_CALLBACK on_failure)
        : key(key), on_success(on_success), on_failure(on_failure) {}
    virtual void DoIt(typename ET::T_CONTAINER::type& container, typename ET::T_STREAM_TYPE&) override {
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
          SetPromiseToNullEntryOrThrow<T_KEY,
                                       T_ENTRY,
                                       T_KEY_NOT_FOUND_EXCEPTION,
                                       false  // Was `T_POLICY::allow_nonthrowing_get>::DoIt(key, pr);`
                                       >::DoIt(key, pr);
        }
      }
    }
  };

  struct MQMessageAdd : ET::T_MQ_MESSAGE {
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
    virtual void DoIt(typename ET::T_CONTAINER::type& container, typename ET::T_STREAM_TYPE& stream) override {
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
  typename ET::T_MQ& mq_;
};

// TODO(dkorolev): This should be polymorphic too. Using `bricks::metaprogramming::combine`.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct PolymorphicStorage {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

template <typename ENTRY_BASE_TYPE, typename ENTRY>
struct PolymorphicStorage<ENTRY_BASE_TYPE, std::tuple<KeyEntry<ENTRY>>>
    : Storage<ENTRY_BASE_TYPE, KeyEntry<ENTRY>> {
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, std::tuple<KeyEntry<ENTRY>>> ET;

  static_assert(std::is_base_of<ENTRY_BASE_TYPE, ENTRY>::value,
                "The first template parameter for `yoda::API` should be the base class for entry types.");

  /*
  // TODO(dkorolev): Give it a big thought. Right now `ENTRY_BASE_TYPE` is *NOT* visitable,
  // otherwise the `visitor` implementation breaks due to ambiguous function resolution.
  static_assert(
      std::is_base_of<bricks::metaprogramming::abstract_visitable<typename ET::T_VISITABLE_TYPES_AS_TUPLE>,
                      ENTRY_BASE_TYPE>::value,
      "Entry type(s) for `yoda::API` should be `abstract_visitable<>` by the types used"
      " in the API being instantiated. Please make your entry type(s) derive from `visitable<>`.");
  */

  PolymorphicStorage() = delete;
  explicit PolymorphicStorage(typename ET::T_MQ& mq) : Storage<ENTRY_BASE_TYPE, KeyEntry<ENTRY>>(mq) {}
};

// TODO(dkorolev): Base classes list should eventually go via `bricks::metaprogramming::combine<>`.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
class API : public PolymorphicStorage<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> {
 private:
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");

 public:
  typedef EssentialTypedefs<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> ET;

  API() = delete;
  API(const std::string& stream_name)
      : PolymorphicStorage<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE>(mq_),
        stream_(sherlock::Stream<std::unique_ptr<typename ET::T_ENTRY_BASE_TYPE>>(stream_name)),
        mq_listener_(container_, stream_),
        mq_(mq_listener_),
        sherlock_listener_(mq_),
        listener_scope_(stream_.Subscribe(sherlock_listener_)) {}

  typename ET::T_STREAM_TYPE& UnsafeStream() { return stream_; }

  template <typename F>
  typename ET::template T_STREAM_LISTENER_TYPE<F> Subscribe(F& listener) {
    return std::move(stream_.Subscribe(listener));
  }

  // For testing purposes.
  bool CaughtUp() const { return sherlock_listener_.caught_up_; }
  size_t EntriesSeen() const { return sherlock_listener_.entries_seen_; }

 private:
  typename ET::T_STREAM_TYPE stream_;
  typename ET::T_CONTAINER::type container_;
  typename ET::T_MQ_LISTENER mq_listener_;
  typename ET::T_MQ mq_;
  typename ET::T_SHERLOCK_LISTENER sherlock_listener_;
  typename ET::T_SHERLOCK_LISTENER_SCOPE_TYPE listener_scope_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_YODA_H
