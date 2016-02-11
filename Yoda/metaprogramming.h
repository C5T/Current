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
#include <utility>

#include "types.h"
#include "sfinae.h"

#include "../Sherlock/sherlock.h"

#include "../Blocks/MMQ/mmq.h"

#include "../Bricks/template/metaprogramming.h"
#include "../Bricks/template/decay.h"
#include "../Bricks/template/weed.h"

#include "../Bricks/util/future.h"

namespace yoda {

struct Padawan;
namespace MP = current::metaprogramming;

// The container to keep the internal represenation of a particular entry type and all access methods
// implemented via corresponding `operator()` overload.
// Note that the template parameter for should be wrapped one, ex. `Dictionary<MyEntry>`, instead of `MyEntry`.
// Particular implementations are located in `api/*/*.h`.
template <typename YT, typename ENTRY>
struct Container {};

// An abstract type to derive message queue message types from.
// All asynchronous events within one Yoda instance go through this message queue.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct MQMessage;

// Sherlock stream listener, responsible for converting every stream entry into a message queue one.
// Encapsulates RTTI dynamic dispatching to bring all corresponding containers up-to-date.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct StreamListener;

// Message queue listener: makes sure each message gets its `virtual Process()` method called,
// in the right order of processing messages.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct MQListener;

struct YodaTypesBase {};  // For `static_assert(std::is_base_of<...>)` compile-time checks.

template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct YodaTypes : YodaTypesBase {
  static_assert(IsTypeList<SUPPORTED_TYPES_LIST>::value, "");

  using T_SUPPORTED_TYPES_LIST = SUPPORTED_TYPES_LIST;

  template <typename T>
  using SherlockEntryTypeFromYodaEntryType = typename T::T_SHERLOCK_TYPES;
  // Need to flatten all the types here, so no `TypeListImpl<>`, only `SlowTypeList`.
  using T_UNDERLYING_TYPES_LIST =
      SlowTypeList<MP::map<SherlockEntryTypeFromYodaEntryType, T_SUPPORTED_TYPES_LIST>>;

  using T_MQ_LISTENER = MQListener<PERSISTENCE, CLONER, T_SUPPORTED_TYPES_LIST>;
  using T_MQ_MESSAGE_INTERNAL_TYPEDEF = MQMessage<PERSISTENCE, CLONER, T_SUPPORTED_TYPES_LIST>;
  using T_MQ = blocks::MMQ<std::unique_ptr<T_MQ_MESSAGE_INTERNAL_TYPEDEF>, T_MQ_LISTENER>;

  using T_STREAM_TYPE = current::sherlock::StreamInstance<std::unique_ptr<Padawan>, PERSISTENCE, CLONER>;

  using T_SHERLOCK_LISTENER = StreamListener<PERSISTENCE, CLONER, T_SUPPORTED_TYPES_LIST>;
  using T_SHERLOCK_LISTENER_SCOPE_TYPE =
      typename T_STREAM_TYPE::template SyncListenerScope<T_SHERLOCK_LISTENER>;
};

// Since container type depends on MMQ message type and vice versa, they are defined outside `YodaTypes`.
// This enables external users to specify their types in a template-deducible manner.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename YT>
using YodaMMQMessage = MQMessage<PERSISTENCE, CLONER, typename YT::T_SUPPORTED_TYPES_LIST>;

template <typename YT>
struct YodaContainerImpl {
  template <typename T>
  using ContainerType = Container<YT, T>;
  typedef MP::combine<MP::map<ContainerType, typename YT::T_SUPPORTED_TYPES_LIST>> type;
};

template <typename YT>
using YodaContainer = typename YodaContainerImpl<YT>::type;

namespace type_inference {

// Helper types to extract `Accessor` and `Mutator`.
template <typename T>
struct RetrieveAccessor {};
template <typename T>
struct RetrieveMutator {};

}  // namespace type_inference

template <typename YT>
struct YodaData {
  template <typename T, typename... TS>
  using CWT = current::weed::call_with_type<T, TS...>;

  YodaData(YodaContainer<YT>& container, typename YT::T_STREAM_TYPE& stream)
      : container(container), stream(stream) {}

  // Container getter for `Accessor`.
  template <typename T>
  CWT<YodaContainer<YT>, type_inference::RetrieveAccessor<T>> Accessor() const {
    return container(type_inference::RetrieveAccessor<T>());
  }

  // Container getter for `Mutator`.
  template <typename T>
  CWT<YodaContainer<YT>, type_inference::RetrieveMutator<T>, std::reference_wrapper<typename YT::T_STREAM_TYPE>>
  Mutator() const {
    return container(type_inference::RetrieveMutator<T>(), std::ref(stream));
  }

 private:
  YodaContainer<YT>& container;
  typename YT::T_STREAM_TYPE& stream;
};

// The logic to "interleave" updates from Sherlock stream with inbound Yoda API/SDK requests.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct MQMessage {
  typedef YodaTypes<PERSISTENCE, CLONER, SUPPORTED_TYPES_LIST> YT;
  virtual void Process(YodaContainer<YT>& container,
                       YodaData<YT> container_data,
                       typename YT::T_STREAM_TYPE& stream) = 0;
};

// Stream listener is passing entries from the Sherlock stream into the message queue.
template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct StreamListener {
  typedef YodaTypes<PERSISTENCE, CLONER, SUPPORTED_TYPES_LIST> YT;

  // TODO(dkorolev) || TODO(mzhurovich): `std::ref` + `std::reference_wrapper` ?
  explicit StreamListener(typename YT::T_MQ& mq,
                          std::atomic_bool* p_replay_done,
                          std::condition_variable* p_replay_done_cv)
      : mq_(mq), p_replay_done_(p_replay_done), p_replay_done_cv_(p_replay_done_cv) {}

  struct MQMessageEntry : MQMessage<PERSISTENCE, CLONER, typename YT::T_SUPPORTED_TYPES_LIST> {
    std::unique_ptr<Padawan> entry;
    const size_t index;

    MQMessageEntry(std::unique_ptr<Padawan>&& entry, size_t index) : entry(std::move(entry)), index(index) {}

    virtual void Process(YodaContainer<YT>& container, YodaData<YT>, typename YT::T_STREAM_TYPE&) override {
      // TODO(dkorolev): For this call, `entry`, the first parameter, should be `std::move()`-d,
      // but Bricks doesn't support it yet for `RTTIDynamicCall`.
      MP::RTTIDynamicCall<typename YT::T_UNDERLYING_TYPES_LIST>(entry, container, index);
    }
  };

  // Sherlock stream listener call.
  void operator()(std::unique_ptr<Padawan>&& entry, blocks::ss::IndexAndTimestamp current) {
    mq_.Emplace(new MQMessageEntry(std::move(entry), current.index));

    // Eventually, the logic of this API implementation is:
    // * Defer all API requests until the persistent part of the stream is fully replayed,
    // * Allow all API requests after that.
    // TODO(dkorolev): The above is coming soon.
  }

  void ReplayDone() {
    *p_replay_done_ = true;
    p_replay_done_cv_->notify_one();
  }

 private:
  typename YT::T_MQ& mq_;
  std::atomic_bool* p_replay_done_;
  std::condition_variable* p_replay_done_cv_;
};

template <template <typename, typename> class PERSISTENCE, class CLONER, typename SUPPORTED_TYPES_LIST>
struct MQListener {
  typedef YodaTypes<PERSISTENCE, CLONER, SUPPORTED_TYPES_LIST> YT;
  explicit MQListener(YodaContainer<YT>& container,
                      YodaData<YT> container_data,
                      typename YT::T_STREAM_TYPE& stream)
      : container_(container), container_data_(container_data), stream_(stream) {}

  // MMQ consumer call.
  void operator()(std::unique_ptr<YodaMMQMessage<PERSISTENCE, CLONER, YT>>&& message) {
    message->Process(container_, container_data_, stream_);
  }

  YodaContainer<YT>& container_;
  YodaData<YT> container_data_;
  typename YT::T_STREAM_TYPE& stream_;
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

template <template <typename, typename> class PERSISTENCE, class CLONER, typename YT>
struct APICalls {
  static_assert(std::is_base_of<YodaTypesBase, YT>::value, "");

  template <typename T, typename... TS>
  using CWT = current::weed::call_with_type<T, TS...>;

  APICalls() = delete;
  explicit APICalls(typename YT::T_MQ& mq) : mq_(mq) {}

  // Asynchronous user function calling functionality.
  typedef YodaData<YT> T_DATA;
  template <typename RETURN_VALUE>
  using USER_FUNCTION = std::function<RETURN_VALUE(T_DATA container_data)>;

  template <typename RETURN_VALUE>
  struct MQMessageFunction : YodaMMQMessage<PERSISTENCE, CLONER, YT> {
    typedef RETURN_VALUE T_RETURN_VALUE;
    USER_FUNCTION<RETURN_VALUE> function;
    std::promise<RETURN_VALUE> promise;

    MQMessageFunction(USER_FUNCTION<RETURN_VALUE>&& function, std::promise<RETURN_VALUE> pr)
        : function(std::forward<USER_FUNCTION<RETURN_VALUE>>(function)), promise(std::move(pr)) {}

    virtual void Process(YodaContainer<YT>&, T_DATA container_data, typename YT::T_STREAM_TYPE&) override {
      CallAndSetPromiseImpl<RETURN_VALUE>::DoIt(function, container_data, promise);
    }
  };

  template <typename RETURN_VALUE, typename NEXT>
  struct MQMessageFunctionWithNext : YodaMMQMessage<PERSISTENCE, CLONER, YT> {
    typedef RETURN_VALUE T_RETURN_VALUE;
    typedef NEXT T_NEXT;
    USER_FUNCTION<RETURN_VALUE> function;
    NEXT next;
    std::promise<void> promise;

    MQMessageFunctionWithNext(USER_FUNCTION<RETURN_VALUE>&& function, NEXT&& next, std::promise<void> pr)
        : function(std::forward<USER_FUNCTION<RETURN_VALUE>>(function)),
          next(std::forward<NEXT>(next)),
          promise(std::move(pr)) {}

    virtual void Process(YodaContainer<YT>&, T_DATA container_data, typename YT::T_STREAM_TYPE&) override {
      next(function(container_data));
      promise.set_value();
    }
  };

  template <typename TYPED_USER_FUNCTION>
  current::Future<current::decay<CWT<TYPED_USER_FUNCTION, T_DATA>>> Transaction(
      TYPED_USER_FUNCTION&& function) {
    using INTERMEDIATE_TYPE = current::decay<CWT<TYPED_USER_FUNCTION, T_DATA>>;
    std::promise<INTERMEDIATE_TYPE> pr;
    current::Future<INTERMEDIATE_TYPE> future = pr.get_future();
    mq_.Emplace(
        new MQMessageFunction<INTERMEDIATE_TYPE>(std::forward<TYPED_USER_FUNCTION>(function), std::move(pr)));
    return future;
  }

  // TODO(dkorolev): Maybe return the value of the `next` function as a `Future`? :-)
  template <typename TYPED_USER_FUNCTION, typename NEXT_USER_FUNCTION>
  current::Future<void> Transaction(TYPED_USER_FUNCTION&& function, NEXT_USER_FUNCTION&& next) {
    using INTERMEDIATE_TYPE = current::decay<CWT<TYPED_USER_FUNCTION, T_DATA>>;
    std::promise<void> pr;
    current::Future<void> future = pr.get_future();
    mq_.Emplace(new MQMessageFunctionWithNext<INTERMEDIATE_TYPE, NEXT_USER_FUNCTION>(
        std::forward<TYPED_USER_FUNCTION>(function), std::forward<NEXT_USER_FUNCTION>(next), std::move(pr)));
    return future;
  }

  typename YT::T_MQ& mq_;
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_METAPROGRAMMING_H
