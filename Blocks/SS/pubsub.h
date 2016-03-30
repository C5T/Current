/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#ifndef BLOCKS_SS_PUBSUB_H
#define BLOCKS_SS_PUBSUB_H

#include "idx_ts.h"

#include "../../TypeSystem/variant.h"
#include "../../Bricks/time/chrono.h"

namespace current {
namespace ss {

struct GenericPublisher {};

template <typename ENTRY>
struct GenericEntryPublisher : GenericPublisher {};

template <typename IMPL, typename ENTRY>
class EntryPublisher : public GenericEntryPublisher<ENTRY>, public IMPL {
 public:
  template <typename... ARGS>
  explicit EntryPublisher(ARGS&&... args)
      : IMPL(std::forward<ARGS>(args)...) {}
  virtual ~EntryPublisher() {}

  idxts_t Publish(const ENTRY& e, std::chrono::microseconds us = current::time::Now()) {
    return IMPL::DoPublish(e, us);
  }
  idxts_t Publish(ENTRY&& e, std::chrono::microseconds us = current::time::Now()) {
    return IMPL::DoPublish(std::move(e), us);
  }

  // template <typename... ARGS>
  // idxts_t Emplace(ARGS&&... args) {
  //   return IMPL::DoEmplace(std::forward<ARGS>(args)...);
  // }
};

template <typename ENTRY>
struct GenericStreamPublisher {};

template <typename IMPL, typename ENTRY>
class StreamPublisher : public GenericStreamPublisher<ENTRY>, public EntryPublisher<IMPL, ENTRY> {
 public:
  using EntryPublisher<IMPL, ENTRY>::EntryPublisher;
  // I've removed `PublishReplayed()` from `StreamPublisher`. -- D.K.
  // The real differentiation would be `UpdateHead()`. -- D.K.
};

// For `static_assert`-s. Must `decay<>` for template xvalue references support.
template <typename T>
struct IsPublisher {
  static constexpr bool value = std::is_base_of<GenericPublisher, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsEntryPublisher {
  static constexpr bool value =
      std::is_base_of<GenericEntryPublisher<current::decay<E>>, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsStreamPublisher {
  static constexpr bool value =
      std::is_base_of<GenericStreamPublisher<current::decay<E>>, current::decay<T>>::value;
};

enum class EntryResponse { Done = 0, More = 1 };
enum class TerminationResponse { Wait = 0, Terminate = 1 };

struct GenericSubscriber {};

template <typename ENTRY>
struct GenericEntrySubscriber : GenericSubscriber {};

template <typename IMPL, typename ENTRY>
class EntrySubscriber : public GenericEntrySubscriber<ENTRY>, public IMPL {
 public:
  template <typename... ARGS>
  EntrySubscriber(ARGS&&... args)
      : IMPL(std::forward<ARGS>(args)...) {}
  virtual ~EntrySubscriber() {}

  EntryResponse operator()(const ENTRY& e, idxts_t current, idxts_t last) {
    return IMPL::operator()(e, current, last);
  }
  EntryResponse operator()(ENTRY&& e, idxts_t current, idxts_t last) {
    return IMPL::operator()(std::move(e), current, last);
  }

  TerminationResponse Terminate() { return IMPL::Terminate(); }
};

template <typename ENTRY>
struct GenericStreamSubscriber {};

template <typename IMPL, typename ENTRY>
class StreamSubscriber : public GenericStreamSubscriber<ENTRY>, public EntrySubscriber<IMPL, ENTRY> {
 public:
  using EntrySubscriber<IMPL, ENTRY>::EntrySubscriber;
};

// For `static_assert`-s. Must `decay<>` for template xvalue references support.
template <typename T>
struct IsSubscriber {
  static constexpr bool value = std::is_base_of<GenericSubscriber, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsEntrySubscriber {
  static constexpr bool value =
      std::is_base_of<GenericEntrySubscriber<current::decay<E>>, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsStreamSubscriber {
  static constexpr bool value =
      std::is_base_of<GenericStreamSubscriber<current::decay<E>>, current::decay<T>>::value;
};

namespace impl {

template <typename TYPE_SUBSCRIBED_TO, typename STREAM_UNDERLYING_VARIANT>
struct PassEntryToSubscriberOrSkipItImpl {
  static_assert(TypeListContains<typename STREAM_UNDERLYING_VARIANT::T_TYPELIST, TYPE_SUBSCRIBED_TO>::value,
                "Subscribing to the type different from the underlying type requires the underlying type"
                " to be a `Variant<>` containing the subscribed to type as an option.");

  template <typename F, typename E>
  static EntryResponse Dispatch(F&& f, E&& entry, idxts_t current, idxts_t last) {
    static_assert(IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    const E& entry_cref = entry;
    if (Exists<TYPE_SUBSCRIBED_TO>(entry_cref)) {
      return f(Value<TYPE_SUBSCRIBED_TO>(std::forward<E>(entry)), current, last);
    } else {
      return EntryResponse::More;
    }
  }
};

template <typename T>
struct PassEntryToSubscriberOrSkipItImpl<T, T> {
  template <typename F, typename E>
  static EntryResponse Dispatch(F&& f, E&& entry, idxts_t current, idxts_t last) {
    static_assert(IsEntrySubscriber<F, T>::value, "");
    return f(std::forward<E>(entry), current, last);
  }
};

}  // namespace current::ss::impl

template <typename TYPE_SUBSCRIBED_TO, typename STREAM_UNDERLYING_VARIANT, typename F, typename E>
EntryResponse PassEntryToSubscriberOrSkipIt(F&& f, E&& entry, idxts_t current, idxts_t last) {
  return impl::PassEntryToSubscriberOrSkipItImpl<TYPE_SUBSCRIBED_TO, STREAM_UNDERLYING_VARIANT>::Dispatch(
      std::forward<F>(f), std::forward<E>(entry), current, last);
}

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_PUBSUB_H
