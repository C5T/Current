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

#ifndef BRICKS_MQ_INTERFACE_INTERFACE_H
#define BRICKS_MQ_INTERFACE_INTERFACE_H

#include <functional>
#include <utility>

#include "../../template/weed.h"

namespace bricks {
namespace mq {

// TODO(dkorolev): Enables three abstractions for message passing:
//
// 1) A generic call to `operator()`.
//    Enabling one-, two- or three-parameter signatures, returning `bool` or `void`,
//    and accepting entries as const- or rvalue reference parameters.
// 2) A way to do `UpdateHead()` (external sync events or timer ticke),
//    and, perhaps, combine it with the way to notify that previously persisted entries have been replayed.
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
// One usecase is to `std::move()` the received entry into a different message queue.
// If a copy of an entry is passed in, this behavior is impossible without making a clone of the entry.
// At the same time, some framworks may construct the entry object for this particular listener,
// thus making the very clone operation.
// This is not to mention that cloning a polymorphic `unique_ptr<>` may be painful at times.
// Therefore, if the framework is passing in a const reference to the entry, it is also responsible
// for providing a method to clone this entry.
// The client, in their turn, can define a signature with a const reference or with an rvalue reference --
// -- whichever suits their needs best -- and know that the framework will take care of making a copy
// of the incoming entry as necessary.

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
    -> decltype(std::declval<F>()(static_cast<const E&>(std::declval<E>()), 0u, 0u), bool()) {
  return false;
}

template <typename F, typename E>
constexpr static auto RequiresCopyOfEntry2(int)
    -> decltype(std::declval<F>()(static_cast<const E&>(std::declval<E>()), 0u), bool()) {
  return false;
}

template <typename F, typename E>
constexpr static auto RequiresCopyOfEntry1(int)
    -> decltype(std::declval<F>()(static_cast<const E&>(std::declval<E>())), bool()) {
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

}  // namespace bricks::mq::impl

// The interface exposed for the frameworks to pass entries to process down to listeners.

// Generic const reference usecase, which dispatches an entry that should be preserver.
// It will be cloned is the listener requires an rvalue reference. Hence a custom `clone` method is required.
template <typename F, typename E>
inline bool DispatchEntryByConstReference(
    F&& f, const E& e, size_t index, size_t total, std::function<E(const E&)> clone_f) {
  return impl::DispatchEntryMakingACopyIfNecessary<impl::RequiresCopyOfEntry<F, E>(0)>::template DoIt<F, E>(
      std::forward<F>(f), e, index, total, clone_f);
}

// A special case of the above method; GCC doesn't handle default values for `std::function<>` parameters well.
template <typename F, typename E>
inline bool DispatchEntryByConstReference(F&& f, const E& e, size_t index, size_t total) {
  std::function<E(const E&)> clone_f = [](const E& e) { return e; };
  return DispatchEntryByConstReference(std::forward<F>(f), e, index, total, clone_f);
}

// Generic rvalue reference usecase, which dispatches the entry that does not need to be reused later.
template <typename F, typename E>
inline bool DispatchEntryByRValue(F&& f, E&& e, size_t index, size_t total) {
  return impl::DispatchEntryWithoutMakingACopy<F, E>(std::forward<F>(f), std::forward<E>(e), index, total);
}

}  // namespace bricks::mq
}  // namespace bricks

#endif  // BRICKS_MQ_INTERFACE_INTERFACE_H
