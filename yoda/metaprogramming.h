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

// The metaprogramming part of Yoda's implementation.
// Type definitions, hardly any implementation code. Used to enable support per-storage types in a way
// that allows combining them into a single polymorphic storage atop a single polymorphic stream.

#ifndef SHERLOCK_YODA_METAPROGRAMMING_H
#define SHERLOCK_YODA_METAPROGRAMMING_H

#include <functional>
#include <future>
#include <utility>

#include "../sherlock.h"

#include "../../Bricks/template/metaprogramming.h"
#include "../../Bricks/template/weed.h"
#include "../../Bricks/mq/inmemory/mq.h"

namespace yoda {

struct Padawan;

namespace sfinae {
// TODO(dkorolev): Let's move this to Bricks once we merge repositories?
// Associative container type selector. Attempts to use:
// 1) std::unordered_map<T_KEY, T_ENTRY, wrapper for `T_KEY::Hash()`>
// 2) std::unordered_map<T_KEY, T_ENTRY [, std::hash<T_KEY>]>
// 3) std::map<T_KEY, T_ENTRY>
// in the above order.

template <typename T_KEY>
constexpr bool HasHashFunction(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasHashFunction(int) -> decltype(std::declval<T_KEY>().Hash(), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasStdHash(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasStdHash(int) -> decltype(std::hash<T_KEY>(), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasOperatorEquals(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasOperatorEquals(int)
    -> decltype(static_cast<bool>(std::declval<T_KEY>() == std::declval<T_KEY>()), bool()) {
  return true;
}

template <typename T_KEY>
constexpr bool HasOperatorLess(char) {
  return false;
}

template <typename T_KEY>
constexpr auto HasOperatorLess(int)
    -> decltype(static_cast<bool>(std::declval<T_KEY>() < std::declval<T_KEY>()), bool()) {
  return true;
}

template <typename T_KEY, typename T_ENTRY, bool HAS_CUSTOM_HASH_FUNCTION, bool DEFINES_STD_HASH>
struct T_MAP_TYPE_SELECTOR {};

// `T_KEY::Hash()` and `T_KEY::operator==()` are defined, use std::unordered_map<> with user-defined hash
// function.
template <typename T_KEY, typename T_ENTRY, bool DEFINES_STD_HASH>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, true, DEFINES_STD_HASH> {
  static_assert(HasOperatorEquals<T_KEY>(0), "The key type defines `Hash()`, but not `operator==()`.");
  struct HashFunction {
    size_t operator()(const T_KEY& key) const { return static_cast<size_t>(key.Hash()); }
  };
  typedef std::unordered_map<T_KEY, T_ENTRY, HashFunction> type;
};

// `T_KEY::Hash()` is not defined, but `std::hash<T_KEY>` and `T_KEY::operator==()` are, use
// std::unordered_map<>.
template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, false, true> {
  static_assert(HasOperatorEquals<T_KEY>(0),
                "The key type supports `std::hash<T_KEY>`, but not `operator==()`.");
  typedef std::unordered_map<T_KEY, T_ENTRY> type;
};

// Neither `T_KEY::Hash()` nor `std::hash<T_KEY>` are defined, use std::map<>.
template <typename T_KEY, typename T_ENTRY>
struct T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, false, false> {
  static_assert(HasOperatorLess<T_KEY>(0), "The key type defines neither `Hash()` nor `operator<()`.");
  typedef std::map<T_KEY, T_ENTRY> type;
};

template <typename T_KEY, typename T_ENTRY>
using T_MAP_TYPE =
    typename T_MAP_TYPE_SELECTOR<T_KEY, T_ENTRY, HasHashFunction<T_KEY>(0), HasStdHash<T_KEY>(0)>::type;

// The best way I found to have clang++ dump the actual type in error message. -- D.K.
// Usage: static_assert(sizeof(is_same_or_compile_error<A, B>), "");
// TODO(dkorolev): Chat with Max, remove or move it into Bricks.
template <typename T1, typename T2>
struct is_same_or_compile_error {
  char c[std::is_same<T1, T2>::value ? 1 : -1];
};

}  // namespace sfinae

namespace MP = bricks::metaprogramming;

// The container to keep the internal represenation of a particular entry type and all access methods
// implemented via corresponding `operator()` overload.
// Note that the template parameter for should be wrapped one, ex. `KeyEntry<MyEntry>`, instead of `MyEntry`.
// Particular implementations are located in `api/*/*.h`.
template <typename YT, typename ENTRY>
struct Container {};

// An abstract type to derive message queue message types from.
// All asynchronous events within one Yoda instance go through this message queue.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage;

// Sherlock stream listener, responsible for converting every stream entry into a message queue one.
// Encapsulates RTTI dynamic dispatching to bring all corresponding containers up-to-date.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct StreamListener;

// Message queue listener: makes sure each message gets its `virtual Process()` method called,
// in the right order of processing messages.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener;

struct YodaTypesBase {};  // For `static_assert(std::is_base_of<...>)` compile-time checks.

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct YodaTypes : YodaTypesBase {
  static_assert(MP::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");

  typedef SUPPORTED_TYPES_AS_TUPLE T_SUPPORTED_TYPES_AS_TUPLE;

  template <typename T>
  using SherlockEntryTypeFromYodaEntryType = typename T::T_ENTRY;
  typedef MP::map<SherlockEntryTypeFromYodaEntryType, SUPPORTED_TYPES_AS_TUPLE> T_UNDERLYING_TYPES_AS_TUPLE;

  typedef MQListener<T_SUPPORTED_TYPES_AS_TUPLE> T_MQ_LISTENER;
  typedef MQMessage<T_SUPPORTED_TYPES_AS_TUPLE> T_MQ_MESSAGE_INTERNAL_TYPEDEF;
  typedef bricks::mq::MMQ<std::unique_ptr<T_MQ_MESSAGE_INTERNAL_TYPEDEF>, T_MQ_LISTENER> T_MQ;

  typedef sherlock::StreamInstance<std::unique_ptr<Padawan>> T_STREAM_TYPE;

  typedef StreamListener<T_SUPPORTED_TYPES_AS_TUPLE> T_SHERLOCK_LISTENER;
  typedef typename T_STREAM_TYPE::template SyncListenerScope<T_SHERLOCK_LISTENER>
      T_SHERLOCK_LISTENER_SCOPE_TYPE;
};

// Since container type depends on MMQ message type and vice versa, they are defined outside `YodaTypes`.
// This enables external users to specify their types in a template-deducible manner.
template <typename YT>
using YodaMMQMessage = MQMessage<typename YT::T_SUPPORTED_TYPES_AS_TUPLE>;

template <typename YT>
struct YodaContainerImpl {
  template <typename T>
  using ContainerType = Container<YT, T>;
  typedef MP::combine<MP::map<ContainerType, typename YT::T_SUPPORTED_TYPES_AS_TUPLE>> type;
};

template <typename YT>
using YodaContainer = typename YodaContainerImpl<YT>::type;

namespace container_wrapper {
template <typename T>
struct RetrieveAccessor {};
template <typename T>
struct RetrieveMutator {};

struct Get {};
struct Add {};
}  // namespace container_wrapper

template <typename YT>
struct ContainerWrapper {
  template <typename T, typename... TS>
  using CWT = bricks::weed::call_with_type<T, TS...>;

  ContainerWrapper(YodaContainer<YT>& container, typename YT::T_STREAM_TYPE& stream)
      : container(container), stream(stream) {}

  // Container getter for `Accessor`.
  template <typename T>
  CWT<YodaContainer<YT>, container_wrapper::RetrieveAccessor<T>> Accessor() const {
    return container(container_wrapper::RetrieveAccessor<T>());
  }

  // Container getter for `Mutator`.
  template <typename T>
  CWT<YodaContainer<YT>,
      container_wrapper::RetrieveMutator<T>,
      std::reference_wrapper<typename YT::T_STREAM_TYPE>>
  Mutator() const {
    return container(container_wrapper::RetrieveMutator<T>(), std::ref(stream));
  }

 private:
  YodaContainer<YT>& container;
  typename YT::T_STREAM_TYPE& stream;
};

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct StreamListener {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;

  explicit StreamListener(typename YT::T_MQ& mq) : caught_up_(false), entries_seen_(0u), mq_(mq) {}

  struct MQMessageEntry : MQMessage<typename YT::T_SUPPORTED_TYPES_AS_TUPLE> {
    std::unique_ptr<Padawan> entry;

    explicit MQMessageEntry(std::unique_ptr<Padawan>&& entry) : entry(std::move(entry)) {}

    virtual void Process(YodaContainer<YT>& container,
                         ContainerWrapper<YT>,
                         typename YT::T_STREAM_TYPE&) override {
      MP::RTTIDynamicCall<typename YT::T_UNDERLYING_TYPES_AS_TUPLE>(entry, container);
    }
  };

  // Sherlock stream listener call.
  bool Entry(std::unique_ptr<Padawan>& entry, size_t index, size_t total) {
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

  std::atomic_bool caught_up_;
  std::atomic_size_t entries_seen_;

 private:
  typename YT::T_MQ& mq_;
};

// The logic to "interleave" updates from Sherlock stream with inbound Yoda API/SDK requests.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;
  virtual void Process(YodaContainer<YT>& container,
                       ContainerWrapper<YT> container_wrapper,
                       typename YT::T_STREAM_TYPE& stream) = 0;
};

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;
  explicit MQListener(YodaContainer<YT>& container,
                      ContainerWrapper<YT> container_wrapper,
                      typename YT::T_STREAM_TYPE& stream)
      : container_(container), container_wrapper_(container_wrapper), stream_(stream) {}

  // MMQ consumer call.
  void OnMessage(std::unique_ptr<YodaMMQMessage<YT>>&& message) {
    message->Process(container_, container_wrapper_, stream_);
  }

  YodaContainer<YT>& container_;
  ContainerWrapper<YT> container_wrapper_;
  typename YT::T_STREAM_TYPE& stream_;
};

template <typename YT, typename CONCRETE_TYPE>
struct YodaImpl {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  YodaImpl() = delete;
};

template <typename YT, typename SUPPORTED_TYPES_AS_TUPLE>
struct CombinedYodaImpls {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  static_assert(MP::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

// Shamelessly taken from `Bricks/template/combine.h`.
// TODO(dkorolev): With Max, think of how to best move these into Bricks, or at least rename a few.
template <typename YT, typename T>
struct dispatch {
  T instance;

  template <typename... XS>
  static constexpr bool sfinae(char) {
    return false;
  }

  template <typename... XS>
  static constexpr auto sfinae(int) -> decltype(std::declval<T>()(std::declval<XS>()...), bool()) {
    return true;
  }

  template <typename... XS>
  typename std::enable_if<sfinae<XS...>(0), decltype(std::declval<T>()(std::declval<XS>()...))>::type
  operator()(XS&&... params) {
    return instance(std::forward<XS>(params)...);
  }

  dispatch() = delete;
  explicit dispatch(typename YT::T_MQ& mq) : instance(mq) {}
};

template <typename YT, typename T1, typename T2>
struct inherit_from_both : T1, T2 {
  using T1::operator();
  using T2::operator();
  inherit_from_both() = delete;
  explicit inherit_from_both(typename YT::T_MQ& mq) : T1(mq), T2(mq) {}
  // Note: clang++ passes `operator`-s through
  // by default, whereas g++ requires `using`-s. --  D.K.
};

template <typename YT, typename T, typename... TS>
struct CombinedYodaImpls<YT, std::tuple<T, TS...>>
    : inherit_from_both<YT, dispatch<YT, YodaImpl<YT, T>>, CombinedYodaImpls<YT, std::tuple<TS...>>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  CombinedYodaImpls() = delete;
  explicit CombinedYodaImpls(typename YT::T_MQ& mq)
      : inherit_from_both<YT, dispatch<YT, YodaImpl<YT, T>>, CombinedYodaImpls<YT, std::tuple<TS...>>>(mq) {}
};

template <typename YT, typename T>
struct CombinedYodaImpls<YT, std::tuple<T>> : dispatch<YT, YodaImpl<YT, T>> {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  CombinedYodaImpls() = delete;
  explicit CombinedYodaImpls(typename YT::T_MQ& mq) : dispatch<YT, YodaImpl<YT, T>>(mq) {}
};

// All the user-facing methods are defined here.
// To add a new user-facing method, add a new empty struct into `namespace apicalls`
// and a SFINAE-based wrapper into `struct APICallsWrapper`.
namespace apicalls {

// Helper types for user-facing API calls.
struct Get {};
struct AsyncGet {};
struct Add {};
struct AsyncAdd {};

/// TODO(dkorolev): Remove this code.
/// struct AsyncCallFunction {};

template <typename YT, typename API>
struct APICallsWrapper {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");

  template <typename T, typename... TS>
  using CWT = bricks::weed::call_with_type<T, TS...>;

  APICallsWrapper() = delete;
  explicit APICallsWrapper(typename YT::T_MQ& mq) : api(mq) {}

  // User-facing API calls, proxied to the chain of per-type Yoda API-s
  // via the `using T1::operator(); using T2::operator();` trick.
  template <typename... XS>
  CWT<API, apicalls::Get, XS...> Get(XS&&... xs) {
    return api(apicalls::Get(), std::forward<XS>(xs)...);
  }

  template <typename... XS>
  CWT<API, apicalls::Add, XS...> Add(XS&&... xs) {
    return api(apicalls::Add(), std::forward<XS>(xs)...);
  }

  template <typename... XS>
  CWT<API, apicalls::AsyncGet, XS...> AsyncGet(XS&&... xs) {
    return api(apicalls::AsyncGet(), std::forward<XS>(xs)...);
  }

  template <typename... XS>
  CWT<API, apicalls::AsyncAdd, XS...> AsyncAdd(XS&&... xs) {
    return api(apicalls::AsyncAdd(), std::forward<XS>(xs)...);
  }

  /// TODO(dkorolev): Remove this code.
  /// template <typename... XS>
  /// CWT<API, apicalls::AsyncCallFunction, XS...> AsyncCallFunction(XS&&... xs) {
  ///   return api(apicalls::AsyncCallFunction(), xs...);
  /// }

  API api;
};

}  // namespace apicalls

// Handle `void` and non-`void` types equally for promises.
template <typename R>
struct CallAndSetPromiseImpl {
  template <typename FUNCTION, typename PARAMETER>
  static void DoIt(FUNCTION&& function, PARAMETER&& parameter, std::promise<R>& promise) {
    promise.set_value(function(std::forward<PARAMETER>(parameter)));
  }
};

template <>
struct CallAndSetPromiseImpl<void> {
  template <typename FUNCTION, typename PARAMETER>
  static void DoIt(FUNCTION&& function, PARAMETER&& parameter, std::promise<void>& promise) {
    function(std::forward<PARAMETER>(parameter));
    promise.set_value();
  }
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_METAPROGRAMMING_H
