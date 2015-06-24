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

#ifndef BLOCKS_SS_SS_H
#define BLOCKS_SS_SS_H

#include <functional>
#include <utility>

#include "../../Bricks/util/clone.h"
#include "../../Bricks/template/weed.h"

namespace blocks {
namespace ss {

// TODO(dkorolev): Enables three abstractions for message passing:
//
// 1) A generic call to `operator()`.
//    Enabling one-, two- or three-parameter signatures, returning `bool` or `void`,
//    and accepting entries as const- or rvalue reference parameters.
//
// 2) A way to do `UpdateHead()` (external sync events or timer ticks), and,
//    perhaps, combine it with the way to notify that previously persisted entries have been replayed.
//
// 3) A way to do `Terminate()` gracefully (this one has been done already, just need to move here).
//
// TODO(dkorolev): Only the first part is done now. But this code change should make the rest straightforward.

// Invoke the right `operator()` callback.
// There are twelve options possible, 3 * 2 * 2.
//
// Enumerating them as three A-s, two B-s and two C-s:
//
// A: Number of parameters.
// A.1: `void/bool operator()(entry)`, one-parameter signature.
// A.2: `void/bool operator()(entry, index)`, two-parameters signature.
// A.3: `void/bool operator()(entry, index, total)`, three-parameters signature.
// By omitting the last parameters, the user declares they do not need the values of them.
//
// B: Return type.
// B.1: `void operator()(entry [, index [, total]])`.
// B.2: `bool operator()(entry [, index [, total]])`.
// The signature that returns `bool` may indicate that it is done processing entries,
// and that the framework should stop feeding it with the new ones.
// The signature returning `void` defines a listener that will never itself make a decision to stop
// accepting entries. It can be stopped externally (via listener scope), or detached to run forever.
//
// C: Accept a const reference to an entry, or require a copy of which the listener will gain ownership.
// C.1: `void/bool operator()(const T_ENTRY& entry [, index [, total]])`.
// C.2: `void/bool operator()(T_ENTRY&& [, index [, total]])`.
// One listener's usecase is to `std::move()` the received entry into a different message queue.
// If the entry is passed in via a const reference, its ownership can not be transferred to the listener.
// A clone of the entry is made in this case, for the listener to own. At the same time framworks
// that construct the very entry object independently for a particular listener (for example, by
// de-serealizing the entry object from an `std::string`) they pass the entry by an rvalue via `std::move()`,
// which eliminates unnecessary copy/clone operation. This is not to mention that cloning
// a polymorphic `unique_ptr<>` may be painful at times. Therefore, if the framework is passing in
// a const reference to the entry, it is also responsible for providing a method to clone it.
// The client, in their turn, can define a signature with a const reference or with an rvalue reference --
// -- whichever suits their needs best -- and know that the framework will take care of making a copy
// or sparing it as necessary.

// === `operator()` and message processing. ===
// Various signatures according to which the user may choose to receive the messages to process.
namespace impl {

template <typename T, typename... TS>
using CW = bricks::weed::call_with<T, TS...>;

template <typename T, typename... TS>
using CWT = bricks::weed::call_with_type<T, TS...>;

template <int N, typename F, typename E>
struct CallWithNParameters;

template <typename F, typename E>
struct CallWithNParameters<3, F, E> {
  static CWT<F, E, size_t, size_t> CallIt(F&& f, E&& e, size_t index, size_t total) {
    return f(std::forward<E>(e), index, total);
  }
};

template <typename F, typename E>
struct CallWithNParameters<2, F, E> {
  static CWT<F, E, size_t> CallIt(F&& f, E&& e, size_t index, size_t) { return f(std::forward<E>(e), index); }
};

template <typename F, typename E>
struct CallWithNParameters<1, F, E> {
  static CWT<F, E> CallIt(F&& f, E&& e, size_t, size_t) { return f(std::forward<E>(e)); }
};

template <bool N1, bool N2, bool N3, typename F, typename E>
struct FindMatchingSignature {};

template <typename F, typename E>
struct FindMatchingSignature<true, false, false, F, E> : CallWithNParameters<1, F, E> {
  enum { valid = true };
};
template <typename F, typename E>
struct FindMatchingSignature<false, true, false, F, E> : CallWithNParameters<2, F, E> {
  enum { valid = true };
};
template <typename F, typename E>
struct FindMatchingSignature<false, false, true, F, E> : CallWithNParameters<3, F, E> {
  enum { valid = true };
};

template <typename F, typename E>
struct CallMatchingSignature : FindMatchingSignature<CW<F, E>::implemented,
                                                     CW<F, E, size_t>::implemented,
                                                     CW<F, E, size_t, size_t>::implemented,
                                                     F,
                                                     E> {};

template <typename R, typename F, typename E>
struct BoolOrTrueImpl;

template <typename F, typename E>
struct BoolOrTrueImpl<bool, F, E> {
  static bool MakeItBool(F&& f, E&& e, size_t index, size_t total) {
    static_assert(CallMatchingSignature<F, E>::valid,
                  "The listener should expose only one signature of `operator()`.");
    return CallMatchingSignature<F, E>::CallIt(std::forward<F>(f), std::forward<E>(e), index, total);
  };
};

template <typename F, typename E>
struct BoolOrTrueImpl<void, F, E> {
  static bool MakeItBool(F&& f, E&& e, size_t index, size_t total) {
    CallMatchingSignature<F, E>::CallIt(std::forward<F>(f), std::forward<E>(e), index, total);
    return true;
  };
};

template <typename F, typename E>
struct BoolOrTrue {
  static bool DoIt(F&& f, E&& e, size_t index, size_t total) {
    return BoolOrTrueImpl<
        decltype(CallMatchingSignature<F, E>::CallIt(std::declval<F>(), std::declval<E>(), 0, 0)),
        F,
        E>::MakeItBool(std::forward<F>(f), std::forward<E>(e), index, total);
  }
};

template <typename F, typename E>
inline bool DispatchEntryToTheRightSignature(F&& f, E&& e, size_t index, size_t total) {
  return BoolOrTrue<F, E>::DoIt(std::forward<F>(f), std::forward<E>(e), index, total);
}

template <typename F, typename E>
inline bool DispatchActualEntry(F&& f, E&& e, size_t index, size_t total) {
  return DispatchEntryToTheRightSignature(std::forward<F>(f), std::forward<E>(e), index, total);
}

template <typename F, typename E>
inline bool DispatchEntryWithoutMakingACopy(F&& f, E&& e, size_t index, size_t total) {
  return DispatchActualEntry(std::forward<F>(f), std::forward<E>(e), index, total);
}

template <typename F, typename E>
inline bool DispatchEntryWithMakingACopy(
    F&& f, const E& e, size_t index, size_t total, std::function<E(const E&)> clone_f) {
  return DispatchActualEntry(std::forward<F>(f), std::move(clone_f(e)), index, total);
}

template <typename F, typename E>
constexpr static bool RequiresCopyOfEntry3(char) {
  return true;
}

template <typename F, typename E>
constexpr static bool RequiresCopyOfEntry2(char) {
  return true;
}

template <typename F, typename E>
constexpr static bool RequiresCopyOfEntry1(char) {
  return true;
}

template <typename F, typename E>
constexpr static auto RequiresCopyOfEntry3(int)
    -> decltype(std::declval<F>()(std::declval<const E&>(), 0u, 0u), bool()) {
  return false;
}

template <typename F, typename E>
constexpr static auto RequiresCopyOfEntry2(int)
    -> decltype(std::declval<F>()(std::declval<const E&>(), 0u), bool()) {
  return false;
}

template <typename F, typename E>
constexpr static auto RequiresCopyOfEntry1(int)
    -> decltype(std::declval<F>()(std::declval<const E&>()), bool()) {
  return false;
}

template <typename F, typename E>
constexpr static bool RequiresCopyOfEntry(int) {
  return RequiresCopyOfEntry3<F, E>(0) && RequiresCopyOfEntry2<F, E>(0) && RequiresCopyOfEntry1<F, E>(0);
}
template <bool REQUIRES_COPY>
struct DispatchEntryMakingACopyIfNecessary;

template <>
struct DispatchEntryMakingACopyIfNecessary<true> {
  template <typename F, typename E>
  static bool DoIt(F&& f, const E& e, size_t index, size_t total, std::function<E(const E&)> clone_f) {
    return DispatchEntryWithMakingACopy<F, E>(std::forward<F>(f), e, index, total, clone_f);
  }
};

template <>
struct DispatchEntryMakingACopyIfNecessary<false> {
  template <typename F, typename E>
  static bool DoIt(F&& f, const E& e, size_t index, size_t total, std::function<E(const E&)>) {
    return DispatchEntryWithoutMakingACopy(std::forward<F>(f), e, index, total);
  }
};

}  // namespace blocks::ss::impl

// The interface exposed for the frameworks to pass entries to process down to listeners.

// Generic const reference usecase, which dispatches an entry that should be preserved.
// It will be cloned if the listener requires an rvalue reference. Hence a custom `clone` method is required.
template <typename F, typename E>
inline bool DispatchEntryByConstReference(
    F&& f, const E& e, size_t index, size_t total, std::function<E(const E&)> clone_f) {
  return impl::DispatchEntryMakingACopyIfNecessary<impl::RequiresCopyOfEntry<F, E>(0)>::template DoIt<F, E>(
      std::forward<F>(f), e, index, total, clone_f);
}

// A special case of the above method; GCC doesn't handle default values for `std::function<>` parameters well.
template <typename F, typename E>
inline bool DispatchEntryByConstReference(F&& f, const E& e, size_t index, size_t total) {
  return DispatchEntryByConstReference(std::forward<F>(f), e, index, total, bricks::DefaultCloneFunction<E>());
}

// Generic rvalue reference usecase, which dispatches the entry that does not need to be reused later.
template <typename F, typename E>
inline bool DispatchEntryByRValue(F&& f, E&& e, size_t index, size_t total) {
  return impl::DispatchEntryWithoutMakingACopy<F, E>(std::forward<F>(f), std::forward<E>(e), index, total);
}

// === `Publisher<IMPL, ENTRY>` ===
// The generic interface to publish an entry to a stream of any kind.
// For this interface, there is no difference between FileAppender, MMQ or a Sherlock stream,
// as long as they accept entries to be published.
// The main purpose of this wrapper is to eliminate unnecessary code and data copies. Specifically:
// 1) To allow publishing a `const ENTRY&` into a stream that only accepts rvalue references.
//    (The default clone method will be used.)
// 2) To enable `EmplaceEntry()` for streams that only expose publish methods taking the entry itself.
//    (The entry will be constructed by the wrapper and `std::move()`-d away into the stream.)
// The convention is to return the 1-base index of the entry in the stream as `size_t`,
// or zero if the entry can not be published (for example, if it has been discarded from an MMQ -- D.K.).

struct GenericPublisher {};
template <typename ENTRY>
struct GenericEntryPublisher : GenericPublisher {};

template <typename IMPL, typename ENTRY>
class Publisher : public GenericEntryPublisher<ENTRY>, public IMPL {
 public:
  // The best option to make this generic code build universally is to have `clone`
  // always passed in explicitly as the first parameter. -- D.K.
  template <typename... EXTRA_PARAMS>
  explicit Publisher(std::function<ENTRY(const ENTRY&)> clone, EXTRA_PARAMS&&... extra_params)
      : IMPL(clone, std::forward<EXTRA_PARAMS>(extra_params)...) {}

  // Deliverately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  inline size_t Publish(const ENTRY& e) {
    return IMPL::DoPublish(e);
  }
  inline size_t Publish(ENTRY&& e) {
    return IMPL::DoPublish(std::move(e));
  }

  template <typename... ARGS>
  inline size_t Emplace(ARGS&&... args) {
    // TODO(dkorolev): SFINAE to support the case when `IMPL::DoPublishByConstReference()` should be used.
    return IMPL::DoEmplace(std::forward<ARGS>(args)...);
  }

  // Special case of publishing `const DERIVED&` or `const std::unique_ptr<DERIVED>&` into a stream
  // of `std::unique_ptr<ENTRY>`, where `ENTRY` is the base class for `DERIVED`.
  template <typename DERIVED>
  typename std::enable_if<bricks::can_be_stored_in_unique_ptr<ENTRY, DERIVED>::value, size_t>::type Publish(
      const DERIVED& e) {
    return IMPL::DoPublishDerived(e);
  }

  template <typename DERIVED>
  typename std::enable_if<bricks::can_be_stored_in_unique_ptr<ENTRY, DERIVED>::value, size_t>::type Publish(
      const std::unique_ptr<DERIVED>& e) {
    assert(e);
    return IMPL::DoPublishDerived(*e.get());
  }
};

// For `static_assert`-s.
template <typename T>
struct IsPublisher {
  static constexpr bool value = std::is_base_of<GenericPublisher, T>::value;
};

template <typename T, typename E>
struct IsEntryPublisher {
  static constexpr bool value = std::is_base_of<GenericEntryPublisher<E>, T>::value;
};

// === `Terminate()` ===
// If the listener implements `Terminate()` it is invoked as the listening is about to end.
// The implementation of `Terminate()` in the listener can return `void` or `bool`.
// The `bool` one can indicate it refuses to terminate just now by returning `false`.
// It is then the responsibility of the listener to eventually terminate.

// TODO(dkorolev): This code is moved from "Sherlock/sherlock". I have yet to refactor/test it.

namespace impl {

template <typename T>
constexpr bool HasTerminateMethod(char) {
  return false;
}

template <typename T>
constexpr auto HasTerminateMethod(int) -> decltype(std::declval<T>().Terminate(), bool()) {
  return true;
}

template <typename T, bool>
struct CallTerminateImpl {
  static bool DoIt(T) { return true; }
};

template <typename T>
struct CallTerminateImpl<T, true> {
  static decltype(std::declval<T>().Terminate()) DoIt(T&& ref) { return ref.Terminate(); }
};

template <typename T, bool>
struct CallTerminateAndReturnBoolImpl {
  static bool DoIt(T&& ref) {
    return CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::forward<T>(ref));
  }
};

template <typename T>
struct CallTerminateAndReturnBoolImpl<T, true> {
  static bool DoIt(T&& ref) {
    CallTerminateImpl<T, HasTerminateMethod<T>(0)>::DoIt(std::forward<T>(ref));
    return true;
  }
};

}  // namespace blocks::ss::impl

template <typename T>
bool CallTerminate(T&& ref) {
  return impl::CallTerminateAndReturnBoolImpl<
      T,
      std::is_same<void,
                   decltype(impl::CallTerminateImpl<T, impl::HasTerminateMethod<T>(0)>::DoIt(
                       std::declval<T>()))>::value>::DoIt(std::forward<T>(ref));
}

// === `ReplayDone()` ===
// Optionally invokes `ReplayDone()` if it is defined by the listener.

namespace impl {

template <typename T>
constexpr bool HasReplayDoneMethod(char) {
  return false;
}

template <typename T>
constexpr auto HasReplayDoneMethod(int) -> decltype(std::declval<T>().ReplayDone(), bool()) {
  return true;
}

template <typename T, bool>
struct CallReplayDoneImpl {
  static void DoIt(T) {}
};

template <typename T>
struct CallReplayDoneImpl<T, true> {
  static decltype(std::declval<T>().ReplayDone()) DoIt(T&& ref) { ref.ReplayDone(); }
};

}  // namespace blocks::ss::impl

template <typename T>
void CallReplayDone(T&& ref) {
  impl::CallReplayDoneImpl<T, impl::HasReplayDoneMethod<T>(0)>::DoIt(std::forward<T>(ref));
}

}  // namespace blocks::ss
}  // namespace blocks

#endif  // BLOCKS_SS_SS_H
