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
// Type definitions, hardly any implementation code.
// Used to enable per-vertical (ex. KeyEntry, MatrixEntry) storage types in a way
// that allows combining them into a single polymorphic storage atop a single polymorphic stream.

#ifndef SHERLOCK_YODA_META_YODA_H
#define SHERLOCK_YODA_META_YODA_H

#include "types.h"

#include "../sherlock.h"

#include "../../Bricks/template/metaprogramming.h"
#include "../../Bricks/mq/inmemory/mq.h"

// The best way I found to have clang++ dump the actual type in error message. -- D.K.
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
struct StreamListener;

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage;

template <typename SUPPORTED_TYPES_AS_TUPLE>
struct PolymorphicContainer;

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct YodaTypes {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");

  typedef ENTRY_BASE_TYPE T_ENTRY_BASE_TYPE;
  typedef SUPPORTED_TYPES_AS_TUPLE T_SUPPORTED_TYPES_AS_TUPLE;

  // A hack to extract the type of the (only) supported entry type.
  // TODO(dkorolev): Make this variadic.
  typedef UnderlyingEntryType<
      bricks::rmconstref<decltype(std::get<0>(std::declval<SUPPORTED_TYPES_AS_TUPLE>()))>> HACK_T_ENTRY;

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

  typedef StreamListener<ENTRY_BASE_TYPE, T_SUPPORTED_TYPES_AS_TUPLE> T_SHERLOCK_LISTENER;

  typedef T_STREAM_LISTENER_TYPE<T_SHERLOCK_LISTENER> T_SHERLOCK_LISTENER_SCOPE_TYPE;
};

// The logic to keep the in-memory state of a particular entry type and its internal represenation.
template <typename>
struct Container {};

// The logic to "interleave" updates from Sherlock stream with inbound Yoda API/SDK requests.
// TODO(dkorolev): This guy should be polymorphic too.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQMessage {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  typedef YodaTypes<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> YT;
  virtual void Process(typename YT::T_CONTAINER::type& container, typename YT::T_STREAM_TYPE& stream) = 0;
};

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct MQListener {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  typedef YodaTypes<ENTRY_BASE_TYPE, SUPPORTED_TYPES_AS_TUPLE> YT;
  explicit MQListener(typename YT::T_CONTAINER::type& container, typename YT::T_STREAM_TYPE& stream)
      : container_(container), stream_(stream) {}

  // MMQ consumer call.
  void OnMessage(std::unique_ptr<typename YT::T_MQ_MESSAGE>& message, size_t dropped_count) {
    // TODO(dkorolev): Should use a non-dropping MMQ here, of course.
    static_cast<void>(dropped_count);  // TODO(dkorolev): And change the method's signature to remove this.
    message->Process(container_, stream_);
  }

  typename YT::T_CONTAINER::type& container_;
  typename YT::T_STREAM_TYPE& stream_;
};

// TODO(dkorolev): This piece of code should also be made polymorphic.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct StreamListener {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

// TODO(dkorolev): Enable the variadic version.
template <typename SUPPORTED_TYPES_AS_TUPLE>
struct PolymorphicContainer {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct YodaImpl {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
  YodaImpl() = delete;
};

// TODO(dkorolev): This should be polymorphic too. Using `bricks::metaprogramming::combine`.
template <typename ENTRY_BASE_TYPE, typename SUPPORTED_TYPES_AS_TUPLE>
struct CombinedYodaImpls {
  static_assert(bricks::metaprogramming::is_std_tuple<SUPPORTED_TYPES_AS_TUPLE>::value, "");
};

}  // namespace yoda

#endif  // SHERLOCK_YODA_META_YODA_H
