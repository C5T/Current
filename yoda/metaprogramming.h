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

#include "types.h"
#include "sfinae.h"
#include "../sherlock.h"

#include "../../Bricks/template/metaprogramming.h"
#include "../../Bricks/template/weed.h"
#include "../../Bricks/mq/inmemory/mq.h"

namespace yoda {

struct Padawan;
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

namespace container_helpers {

// Helper types to extract `Accessor` and `Mutator`.
template <typename T>
struct RetrieveAccessor {};
template <typename T>
struct RetrieveMutator {};

// A wrapper to convert `T` into `KeyEntry<T>`, `MatrixEntry<T>`, etc., using `decltype()`.
// Used to enable top-level `Add()`/`Get()` when passed in the entry only.
template <typename T>
struct ExtractYETFromE {};

// A wrapper to convert `T::T_KEY` into `KeyEntry<T>`, `MatrixEntry<T>`, etc., using `decltype()`.
// Used to enable top-level `Add()`/`Get()` when passed in the entry only.
template <typename K>
struct ExtractYETFromK {};

}  // namespace container_data

template <typename YT>
struct YodaData {
  template <typename T, typename... TS>
  using CWT = bricks::weed::call_with_type<T, TS...>;

  YodaData(YodaContainer<YT>& container, typename YT::T_STREAM_TYPE& stream)
      : container(container), stream(stream) {}

  // Container getter for `Accessor`.
  template <typename T>
  CWT<YodaContainer<YT>, container_helpers::RetrieveAccessor<T>> Accessor() const {
    return container(container_helpers::RetrieveAccessor<T>());
  }

  // Container getter for `Mutator`.
  template <typename T>
  CWT<YodaContainer<YT>, container_helpers::RetrieveMutator<T>, std::reference_wrapper<typename YT::T_STREAM_TYPE>>
  Mutator() const {
    return container(container_helpers::RetrieveMutator<T>(), std::ref(stream));
  }

  // Top-level methods and operators, dispatching by parameter type.
  template <typename E>
  void Add(E&& entry) {
    Mutator<CWT<YodaContainer<YT>, container_helpers::ExtractYETFromE<E>>>().Add(std::forward<E>(entry));
  }

  template <typename K>
  EntryWrapper<typename CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<K>>::T_ENTRY> Get(K&& key) {
    return Accessor<CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<K>>>().Get(std::forward<K>(key));
  }

  template <typename E>
  YodaData& operator<<(E&& entry) {
    Mutator<CWT<YodaContainer<YT>, container_helpers::ExtractYETFromE<E>>>() << std::forward<E>(entry);
    return *this;
  }

  template <typename K>
  typename CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<K>>::T_ENTRY operator[](K&& key) {
    return Accessor<CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<K>>>()[std::forward<K>(key)];
  }

 private:
  YodaContainer<YT>& container;
  typename YT::T_STREAM_TYPE& stream;
};

// The logic to "interleave" updates from Sherlock stream with inbound Yoda API/SDK requests.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;
  virtual void Process(YodaContainer<YT>& container,
                       YodaData<YT> container_data,
                       typename YT::T_STREAM_TYPE& stream) = 0;
};

// Stream listener is passing entries from the Sherlock stream into the message queue.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct StreamListener {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;

  explicit StreamListener(typename YT::T_MQ& mq) : mq_(mq) {}

  struct MQMessageEntry : MQMessage<typename YT::T_SUPPORTED_TYPES_AS_TUPLE> {
    std::unique_ptr<Padawan> entry;
    const size_t index;

    MQMessageEntry(std::unique_ptr<Padawan>&& entry, size_t index) : entry(std::move(entry)), index(index) {}

    virtual void Process(YodaContainer<YT>& container, YodaData<YT>, typename YT::T_STREAM_TYPE&) override {
      // TODO(dkorolev): For this call, `entry`, the first parameter, should be `std::move()`-d,
      // but Bricks doesn't support it yet for `RTTIDynamicCall`.
      MP::RTTIDynamicCall<typename YT::T_UNDERLYING_TYPES_AS_TUPLE>(entry, container, index);
    }
  };

  // Sherlock stream listener call.
  bool Entry(std::unique_ptr<Padawan>& entry, size_t index, size_t total) {
    static_cast<void>(total);

    mq_.EmplaceMessage(new MQMessageEntry(std::move(entry), index));

    // Eventually, the logic of this API implementation is:
    // * Defer all API requests until the persistent part of the stream is fully replayed,
    // * Allow all API requests after that.

    return true;
  }

 private:
  typename YT::T_MQ& mq_;
};

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener {
  typedef YodaTypes<SUPPORTED_TYPES_AS_TUPLE> YT;
  explicit MQListener(YodaContainer<YT>& container,
                      YodaData<YT> container_data,
                      typename YT::T_STREAM_TYPE& stream)
      : container_(container), container_data_(container_data), stream_(stream) {}

  // MMQ consumer call.
  void OnMessage(std::unique_ptr<YodaMMQMessage<YT>>&& message) {
    message->Process(container_, container_data_, stream_);
  }

  YodaContainer<YT>& container_;
  YodaData<YT> container_data_;
  typename YT::T_STREAM_TYPE& stream_;
};

template <typename YT, typename CONCRETE_TYPE>
struct YodaImpl {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  YodaImpl() = delete;
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

template <typename YT, typename SUPPORTED_TYPES_AS_TUPLE>
struct CombinedYodaImpls {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");
  static_assert(MP::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
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

// TODO(dk+mz): Move promises into Bricks.
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

template <typename YT>
struct APICalls {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");

  template <typename DATA, typename YET, typename E>
  struct TopLevelAdd {
    const E entry;
    TopLevelAdd(E&& entry) : entry(std::move(entry)) {}
    void operator()(DATA data) const { YET::Mutator(data).Add(std::move(entry)); }
  };

  template <typename DATA, typename YET, typename K>
  struct TopLevelGet {
    const K key;
    TopLevelGet(K&& key) : key(std::move(key)) {}
    typedef decltype(std::declval<decltype(YET::Accessor(std::declval<DATA>()))>().Get(std::declval<K>())) T_RETVAL;
    T_RETVAL operator()(DATA data) { return YET::Accessor(data).Get(std::move(key)); }
  };

  template <typename T, typename... TS>
  using CWT = bricks::weed::call_with_type<T, TS...>;

  APICalls() = delete;
  explicit APICalls(typename YT::T_MQ& mq) : mq_(mq) {}

  // Asynchronous user function calling functionality.
  typedef YodaData<YT> T_DATA;
  template <typename RETURN_VALUE>
  using T_USER_FUNCTION = std::function<RETURN_VALUE(T_DATA container_data)>;

  template <typename RETURN_VALUE>
  struct MQMessageFunction : YodaMMQMessage<YT> {
    typedef RETURN_VALUE T_RETURN_VALUE;
    T_USER_FUNCTION<T_RETURN_VALUE> function;
    std::promise<T_RETURN_VALUE> promise;

    MQMessageFunction(T_USER_FUNCTION<T_RETURN_VALUE>&& function, std::promise<T_RETURN_VALUE> pr)
        : function(std::forward<T_USER_FUNCTION<T_RETURN_VALUE>>(function)), promise(std::move(pr)) {}

    virtual void Process(YodaContainer<YT>&, T_DATA container_data, typename YT::T_STREAM_TYPE&) override {
      CallAndSetPromiseImpl<T_RETURN_VALUE>::DoIt(function, container_data, promise);
    }
  };

  template <typename RETURN_VALUE, typename NEXT>
  struct MQMessageFunctionWithNext : YodaMMQMessage<YT> {
    typedef RETURN_VALUE T_RETURN_VALUE;
    typedef NEXT T_NEXT;
    T_USER_FUNCTION<T_RETURN_VALUE> function;
    NEXT next;
    std::promise<void> promise;

    MQMessageFunctionWithNext(T_USER_FUNCTION<T_RETURN_VALUE>&& function, NEXT&& next, std::promise<void> pr)
        : function(std::forward<T_USER_FUNCTION<T_RETURN_VALUE>>(function)),
          next(std::forward<NEXT>(next)),
          promise(std::move(pr)) {}

    virtual void Process(YodaContainer<YT>&, T_DATA container_data, typename YT::T_STREAM_TYPE&) override {
      next(function(container_data));
      promise.set_value();
    }
  };

  template <typename T_TYPED_USER_FUNCTION>
  Future<bricks::rmconstref<CWT<T_TYPED_USER_FUNCTION, T_DATA>>> Transaction(T_TYPED_USER_FUNCTION&& function) {
    using T_INTERMEDIATE_TYPE = bricks::rmconstref<CWT<T_TYPED_USER_FUNCTION, T_DATA>>;
    std::promise<T_INTERMEDIATE_TYPE> pr;
    Future<T_INTERMEDIATE_TYPE> future = pr.get_future();
    // TODO(dkorolev): Figure out the `mq_.EmplaceMessage(new ...)` magic.
    mq_.PushMessage(std::move(make_unique<MQMessageFunction<T_INTERMEDIATE_TYPE>>(
        std::forward<T_TYPED_USER_FUNCTION>(function), std::move(pr))));
    return future;
  }

  // TODO(dkorolev): Maybe return the value of the `next` function as a `Future`? :-)
  template <typename T_TYPED_USER_FUNCTION, typename T_NEXT_USER_FUNCTION>
  Future<void> Transaction(T_TYPED_USER_FUNCTION&& function, T_NEXT_USER_FUNCTION&& next) {
    using T_INTERMEDIATE_TYPE = bricks::rmconstref<CWT<T_TYPED_USER_FUNCTION, T_DATA>>;
    std::promise<void> pr;
    Future<void> future = pr.get_future();
    mq_.EmplaceMessage(new MQMessageFunctionWithNext<T_INTERMEDIATE_TYPE, T_NEXT_USER_FUNCTION>(
        std::forward<T_TYPED_USER_FUNCTION>(function),
        std::forward<T_NEXT_USER_FUNCTION>(next),
        std::move(pr)));
    return future;
  }

  // Helper method to wrap `Add()` into `Transaction()`.
  template <typename ENTRY>
  Future<void> Add(ENTRY&& entry) {
    typedef CWT<YodaContainer<YT>, container_helpers::ExtractYETFromE<ENTRY>> YET;
    return Transaction(TopLevelAdd<YodaData<YT>, YET, ENTRY>(std::forward<ENTRY>(entry)));
  }

  // Helper method to wrap `Get()` into `Transaction()`.
  template <typename KEY>
  Future<EntryWrapper<typename CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<KEY>>::T_ENTRY>> Get(KEY&& key) {
    typedef CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<KEY>> YET;
    return Transaction(TopLevelGet<YodaData<YT>, YET, KEY>(std::forward<KEY>(key)));
  }

  // Helper method to wrap `GetWithNext()` into `Transaction()`.
  template <typename KEY, typename F>
  Future<void> GetWithNext(KEY&& key, F&& f) {
    typedef CWT<YodaContainer<YT>, container_helpers::ExtractYETFromK<KEY>> YET;
    return Transaction(TopLevelGet<YodaData<YT>, YET, KEY>(std::forward<KEY>(key)), std::forward<F>(f));
  }

  typename YT::T_MQ& mq_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_METAPROGRAMMING_H
