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

#include "../../port.h"

#include "idx_ts.h"
#include "types.h"

#include "../../typesystem/variant.h"
#include "../../bricks/time/chrono.h"
#include "../../bricks/sync/locks.h"

namespace current {
namespace ss {

struct GenericPublisher {};

template <typename ENTRY>
struct GenericEntryPublisher : GenericPublisher {};

template <typename IMPL, typename ENTRY>
class EntryPublisher : public GenericEntryPublisher<ENTRY>, public IMPL {
 private:
  using MutexLockStatus = current::locks::MutexLockStatus;

 public:
  template <typename... ARGS>
  explicit EntryPublisher(ARGS&&... args)
      : IMPL(std::forward<ARGS>(args)...) {}

  virtual ~EntryPublisher() {}

  template <MutexLockStatus MLS = MutexLockStatus::NeedToLock,
            typename E,
            class = ENABLE_IF<CanPublish<current::decay<E>, ENTRY>::value>>
  idxts_t Publish(E&& e) {
    return IMPL::template PublisherPublishImpl<MLS>(std::forward<E>(e), current::time::DefaultTimeArgument());
  }

  template <MutexLockStatus MLS = MutexLockStatus::NeedToLock,
            typename E,
            class = ENABLE_IF<CanPublish<current::decay<E>, ENTRY>::value>>
  idxts_t Publish(E&& e, std::chrono::microseconds us) {
    return IMPL::template PublisherPublishImpl<MLS>(std::forward<E>(e), us);
  }

  template <MutexLockStatus MLS = MutexLockStatus::NeedToLock>
  void UpdateHead() {
    IMPL::template PublisherUpdateHeadImpl<MLS>(current::time::DefaultTimeArgument());
  }

  template <MutexLockStatus MLS = MutexLockStatus::NeedToLock>
  void UpdateHead(std::chrono::microseconds us) {
    IMPL::template PublisherUpdateHeadImpl<MLS>(us);
  }

  // NOTE(dkorolev): The publisher ("publishable") has no business knowing the size of the stream (`Empty()`/`Size()`),
  //                 That's what the subscriber ("subscribable") and persister ("iterable") primitives are for.
};

template <typename ENTRY>
struct GenericStreamPublisher {};

template <typename IMPL, typename ENTRY>
class StreamPublisher : public GenericStreamPublisher<ENTRY>, public EntryPublisher<IMPL, ENTRY> {
 public:
  using EntryPublisher<IMPL, ENTRY>::EntryPublisher;
};

// For `static_assert`-s. Must `decay<>` for template xvalue references support.
// TODO(dkorolev): `Variant` stream types, and publishing those?
template <typename T>
struct IsPublisher {
  static constexpr bool value = std::is_base_of<GenericPublisher, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsEntryPublisher {
  static constexpr bool value = std::is_base_of<GenericEntryPublisher<current::decay<E>>, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsStreamPublisher {
  static constexpr bool value = std::is_base_of<GenericStreamPublisher<current::decay<E>>, current::decay<T>>::value;
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

  EntryResponse operator()(const ENTRY& e, idxts_t current, idxts_t last) { return IMPL::operator()(e, current, last); }
  EntryResponse operator()(const std::string& entry_json, uint64_t current_index, idxts_t last) {
    return IMPL::operator()(entry_json, current_index, last);
  }
  EntryResponse operator()(ENTRY&& e, idxts_t current, idxts_t last) {
    return IMPL::operator()(std::move(e), current, last);
  }
  EntryResponse operator()(std::chrono::microseconds ts) { return IMPL::operator()(ts); }

  // If a type-filtered subscriber hits the end which it doesn't see as the last entry does not pass the filter,
  // we need a way to ask that subscriber whether it wants to terminate or continue.
  EntryResponse EntryResponseIfNoMorePassTypeFilter() const { return IMPL::EntryResponseIfNoMorePassTypeFilter(); }
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
  static constexpr bool value = std::is_base_of<GenericEntrySubscriber<current::decay<E>>, current::decay<T>>::value;
};

template <typename T, typename E>
struct IsStreamSubscriber {
  static constexpr bool value = std::is_base_of<GenericStreamSubscriber<current::decay<E>>, current::decay<T>>::value;
};

namespace impl {

template <typename TYPE_SUBSCRIBED_TO, typename STREAM_UNDERLYING_VARIANT>
struct PassEntryToSubscriberIfTypeMatchesImpl {
  static_assert(TypeListContains<typename STREAM_UNDERLYING_VARIANT::typelist_t, TYPE_SUBSCRIBED_TO>::value,
                "Subscribing to the type different from the underlying type requires the underlying type"
                " to be a `Variant<>` containing the subscribed to type as an option.");

  template <typename F, typename G, typename E>
  static EntryResponse Dispatch(F&& f, G&& fallback, E&& entry, idxts_t current, idxts_t last) {
    static_assert(IsEntrySubscriber<F, TYPE_SUBSCRIBED_TO>::value, "");
    const E& entry_cref = entry;
    if (Exists<TYPE_SUBSCRIBED_TO>(entry_cref)) {
      return f(Value<TYPE_SUBSCRIBED_TO>(std::forward<E>(entry)), current, last);
    } else {
      CURRENT_ASSERT(current.index <= last.index);
      if (current.index < last.index) {
        return EntryResponse::More;
      } else {
        return fallback();
      }
    }
  }
};

template <typename T>
struct PassEntryToSubscriberIfTypeMatchesImpl<T, T> {
  template <typename F, typename E, typename G>
  static EntryResponse Dispatch(F&& f, G&&, E&& entry, idxts_t current, idxts_t last) {
    static_assert(IsEntrySubscriber<F, T>::value, "");
    return f(std::forward<E>(entry), current, last);
  }
};

}  // namespace current::ss::impl

template <typename TYPE_SUBSCRIBED_TO, typename STREAM_UNDERLYING_VARIANT, typename F, typename G, typename E>
EntryResponse PassEntryToSubscriberIfTypeMatches(F&& f, G&& fallback, E&& entry, idxts_t current, idxts_t last) {
  return impl::PassEntryToSubscriberIfTypeMatchesImpl<TYPE_SUBSCRIBED_TO, STREAM_UNDERLYING_VARIANT>::Dispatch(
      std::forward<F>(f), std::forward<G>(fallback), std::forward<E>(entry), current, last);
}

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_PUBSUB_H
