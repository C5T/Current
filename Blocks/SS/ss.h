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

#ifndef BLOCKS_SS_SS_H
#define BLOCKS_SS_SS_H

#include <chrono>
#include <utility>

#include "../../Bricks/util/clone.h"
#include "../../Bricks/template/weed.h"

#include "../../TypeSystem/struct.h"

namespace current {
namespace ss {

// `IndexAndTimestamp` structure holds the index of the entry and its timestamp in the stream.
// * `index` is 1-based.
// * Default zero values of `index` and `us` correspond to an invalid state. For example,
//   if `Publish()` method fails for some reason it could `return IndexAndTimestamp()`.
CURRENT_STRUCT(IndexAndTimestamp) {
  CURRENT_FIELD(index, uint64_t);
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_USE_FIELD_AS_TIMESTAMP(us);
  CURRENT_DEFAULT_CONSTRUCTOR(IndexAndTimestamp) : index(0u), us(0) {}
  CURRENT_CONSTRUCTOR(IndexAndTimestamp)(uint64_t index, std::chrono::microseconds us) : index(index), us(us) {}
};

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
// A.2: `void/bool operator()(entry, current_idx_ts)`, two-parameters signature.
// A.3: `void/bool operator()(entry, current_idx_ts, last_idx_ts)`, three-parameters signature.
// By omitting the last parameters, the user declares they do not need the values of them.
//
// B: Return type.
// B.1: `void operator()(entry [, current_idx_ts [, last_idx_ts]])`.
// B.2: `bool operator()(entry [, current_idx_ts [, last_idx_ts]])`.
// The signature that returns `bool` may indicate that it is done processing entries,
// and that the framework should stop feeding it with the new ones.
// The signature returning `void` defines a listener that will never itself make a decision to stop
// accepting entries. It can be stopped externally (via listener scope), or detached to run forever.
//
// C: Accept a const reference to an entry, or require a copy of which the listener will gain ownership.
// C.1: `void/bool operator()(const ENTRY& entry [, current_idx_ts [, last_idx_ts]])`.
// C.2: `void/bool operator()(ENTRY&& [, current_idx_ts [, last_idx_ts]])`.
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
#if 0
namespace impl {

template <typename T, typename... TS>
using CW = current::weed::call_with<T, TS...>;

template <typename T, typename... TS>
using CWT = current::weed::call_with_type<T, TS...>;

using IDX_TS = IndexAndTimestamp;

template <int N>
struct CallWithNParameters;

template <>
struct CallWithNParameters<3> {
  template <typename E, class F>
  static CWT<F, E, IDX_TS, IDX_TS> CallIt(F&& f, E&& e, IDX_TS current, IDX_TS last) {
    return f(std::forward<E>(e), current, last);
  }
};

template <>
struct CallWithNParameters<2> {
  template <typename E, class F>
  static CWT<F, E, IDX_TS> CallIt(F&& f, E&& e, IDX_TS current, IDX_TS) {
    return f(std::forward<E>(e), current);
  }
};

template <>
struct CallWithNParameters<1> {
  template <typename E, class F>
  static CWT<F, E> CallIt(F&& f, E&& e, IDX_TS, IDX_TS) {
    return f(std::forward<E>(e));
  }
};

template <bool N1, bool N2, bool N3>
struct FindMatchingSignature {};

template <>
struct FindMatchingSignature<true, false, false> : CallWithNParameters<1> {
  enum { valid = true };
};
template <>
struct FindMatchingSignature<false, true, false> : CallWithNParameters<2> {
  enum { valid = true };
};
template <>
struct FindMatchingSignature<false, false, true> : CallWithNParameters<3> {
  enum { valid = true };
};

template <typename E, class F>
using CallMatchingSignature = FindMatchingSignature<CW<F, E>::implemented,
                                                    CW<F, E, IDX_TS>::implemented,
                                                    CW<F, E, IDX_TS, IDX_TS>::implemented>;

template <typename ORIGINAL_RETURN_TYPE>
struct BoolOrTrueImpl;

template <>
struct BoolOrTrueImpl<bool> {
  template <typename E, class F>
  static bool MakeItBool(F&& f, E&& e, IDX_TS current, IDX_TS last) {
    static_assert(CallMatchingSignature<E, F>::valid,
                  "The listener should expose only one signature of `operator()`.");
    return CallMatchingSignature<E, F>::CallIt(std::forward<F>(f), std::forward<E>(e), current, last);
  };
};

template <>
struct BoolOrTrueImpl<void> {
  template <typename E, class F>
  static bool MakeItBool(F&& f, E&& e, IDX_TS current, IDX_TS last) {
    CallMatchingSignature<E, F>::CallIt(std::forward<F>(f), std::forward<E>(e), current, last);
    return true;
  };
};

struct BoolOrTrue {
  template <typename E, class F>
  static bool DoIt(F&& f, E&& e, IDX_TS current, IDX_TS last) {
    return BoolOrTrueImpl<decltype(CallMatchingSignature<E, F>::CallIt(
        std::declval<F>(), std::declval<E>(), IDX_TS(), IDX_TS()))>::MakeItBool(std::forward<F>(f),
                                                                                std::forward<E>(e),
                                                                                current,
                                                                                last);
  }
};

template <typename E, class F>
inline bool DispatchEntryToTheRightSignature(F&& f, E&& e, IDX_TS current, IDX_TS last) {
  return BoolOrTrue::DoIt(std::forward<F>(f), std::forward<E>(e), current, last);
}

template <typename E, class F>
inline bool DispatchActualEntry(F&& f, E&& e, IDX_TS current, IDX_TS last) {
  return DispatchEntryToTheRightSignature(std::forward<F>(f), std::forward<E>(e), current, last);
}

template <typename E, class F>
inline bool DispatchEntryWithoutMakingACopy(F&& f, E&& e, IDX_TS current, IDX_TS last) {
  return DispatchActualEntry(std::forward<F>(f), std::forward<E>(e), current, last);
}

template <class CLONER, typename E, class F>
inline bool DispatchEntryWithMakingACopy(F&& f, const E& e, IDX_TS current, IDX_TS last) {
  return DispatchActualEntry(std::forward<F>(f), std::move(CLONER::Clone(e)), current, last);
}

template <typename E, class F>
constexpr static bool RequiresCopyOfEntry3(char) {
  return true;
}

template <typename E, class F>
constexpr static bool RequiresCopyOfEntry2(char) {
  return true;
}

template <typename E, class F>
constexpr static bool RequiresCopyOfEntry1(char) {
  return true;
}

template <typename E, class F>
constexpr static auto RequiresCopyOfEntry3(int)
    -> decltype(std::declval<F>()(std::declval<const E&>(), IDX_TS(), IDX_TS()), bool()) {
  return false;
}

template <typename E, class F>
constexpr static auto RequiresCopyOfEntry2(int)
    -> decltype(std::declval<F>()(std::declval<const E&>(), IDX_TS()), bool()) {
  return false;
}

template <typename E, class F>
constexpr static auto RequiresCopyOfEntry1(int)
    -> decltype(std::declval<F>()(std::declval<const E&>()), bool()) {
  return false;
}

template <typename E, class F>
constexpr static bool RequiresCopyOfEntry(int) {
  return RequiresCopyOfEntry3<E, F>(0) && RequiresCopyOfEntry2<E, F>(0) && RequiresCopyOfEntry1<E, F>(0);
}

template <bool REQUIRES_COPY, class CLONER>
struct DispatchEntryMakingACopyIfNecessary;

template <class CLONER>
struct DispatchEntryMakingACopyIfNecessary<true, CLONER> {
  template <typename E, class F>
  static bool DoIt(F&& f, const E& e, IDX_TS current, IDX_TS last) {
    return DispatchEntryWithMakingACopy<CLONER>(std::forward<F>(f), e, current, last);
  }
};

template <class CLONER>
struct DispatchEntryMakingACopyIfNecessary<false, CLONER> {
  template <typename E, class F>
  static bool DoIt(F&& f, const E& e, IDX_TS current, IDX_TS last) {
    // IMPORTANT: Do not explicitly specify `E` as the template parameter, as it interferes
    // with extended reference resolution rules, creating an unnecessary copy. -- D.K.
    return DispatchEntryWithoutMakingACopy(std::forward<F>(f), e, current, last);
  }
};

}  // namespace current::ss::impl

// The interface exposed for the frameworks to pass entries to process down to listeners.

// Generic const reference usecase, which dispatches an entry that should be preserved. It will
// be cloned if the listener requires an rvalue reference, and custom cloner implementation can be provided.
// Template parameter order is tweaked for more often specified to less often specified parameters.
template <class CLONER = current::DefaultCloner, typename E, typename F>
inline bool DispatchEntryByConstReference(F&& f,
                                          const E& e,
                                          IndexAndTimestamp current,
                                          IndexAndTimestamp last) {
  // IMPORTANT: Do not explicitly specify template parameters to `DoIt`, as they interfere
  // with extended reference resolution rules, resulting in an unnecessary copy. -- D.K.
  return impl::DispatchEntryMakingACopyIfNecessary<impl::RequiresCopyOfEntry<E, F>(0), CLONER>::DoIt(
      std::forward<F>(f), e, current, last);
}

// Generic rvalue reference usecase, which dispatches the entry that does not need to be reused later.
// Template parameter order is tweaked to { E, F }, since `E` has to be specified more often than `F`.
template <typename E, typename F>
inline bool DispatchEntryByRValue(F&& f, E&& e, IndexAndTimestamp current, IndexAndTimestamp last) {
  return impl::DispatchEntryWithoutMakingACopy(std::forward<F>(f), std::forward<E>(e), current, last);
}
#endif

// === `Publisher<IMPL, ENTRY>` ===
// The generic interface to publish an entry to a stream of any kind.
// For this interface, there is no difference between FileAppender, MMQ or a Sherlock stream,
// as long as they accept entries to be published.
// The main purpose of this wrapper is to eliminate unnecessary code and data copies. Specifically:
// 1) To allow publishing a `const ENTRY&` into a stream that only accepts rvalue references.
//    (The default clone method will be used.)
// 2) To enable `EmplaceEntry()` for streams that only expose publish methods taking the entry itself.
//    (The entry will be constructed by the wrapper and `std::move()`-d away into the stream.)
// The convention is to return the 1-base index of the entry in the stream and its timestamp as
// `IndexAndTimestamp`, or zeros if the entry can not be published (for example, if it has been
// discarded from an MMQ -- D.K.).

struct GenericPublisher {};

template <typename ENTRY>
struct GenericEntryPublisher : GenericPublisher {};

template <typename IMPL, typename ENTRY>
class EntryPublisher : public GenericEntryPublisher<ENTRY>, public IMPL {
 public:
  template <typename... ARGS>
  explicit EntryPublisher(ARGS&&... args) : IMPL(std::forward<ARGS>(args)...) {}
  virtual ~EntryPublisher() {}

  // Deliberately keep these two signatures and not one with `std::forward<>` to ensure the type is right.
  inline IndexAndTimestamp Publish(const ENTRY& e) { return IMPL::DoPublish(e); }
  inline IndexAndTimestamp Publish(ENTRY&& e) { return IMPL::DoPublish(std::move(e)); }

  template <typename... ARGS>
  inline IndexAndTimestamp Emplace(ARGS&&... args) {
    // TODO(dkorolev): SFINAE to support the case when `IMPL::DoPublishByConstReference()` should be used.
    return IMPL::DoEmplace(std::forward<ARGS>(args)...);
  }
};

template <typename ENTRY>
struct GenericStreamPublisher {};

template <typename IMPL, typename ENTRY>
class StreamPublisher : public GenericStreamPublisher<ENTRY>, public EntryPublisher<IMPL, ENTRY> {
 public:
  using EntryPublisher<IMPL, ENTRY>::EntryPublisher;
  // Special type of publishing required for replication.
  // Functions below return `false` if the `IndexAndTimestamp` argument provided is inconsistent with the
  // current state of the stream. No publish is performed in this case.
  inline void PublishReplayed(const ENTRY& e, IndexAndTimestamp idx_ts) {
    IMPL::DoPublishReplayed(e, idx_ts);
  }
  inline void PublishReplayed(ENTRY&& e, IndexAndTimestamp idx_ts) {
    IMPL::DoPublishReplayed(std::move(e), idx_ts);
  }
};

#if 0
// Temp mapping for tests.
template <typename IMPL, typename ENTRY>
using Publisher = StreamPublisher<IMPL, ENTRY>;
#endif

// For `static_assert`-s.
template <typename T>
struct IsPublisher {
  static constexpr bool value = std::is_base_of<GenericPublisher, T>::value;
};

template <typename T, typename E>
struct IsEntryPublisher {
  static constexpr bool value = std::is_base_of<GenericEntryPublisher<E>, T>::value;
};

template <typename T, typename E>
struct IsStreamPublisher {
  static constexpr bool value = std::is_base_of<GenericStreamPublisher<E>, T>::value;
};

#if 0
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

}  // namespace current::ss::impl

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

}  // namespace current::ss::impl

template <typename T>
void CallReplayDone(T&& ref) {
  impl::CallReplayDoneImpl<T, impl::HasReplayDoneMethod<T>(0)>::DoIt(std::forward<T>(ref));
}
#endif

enum class EntryResponse { Done = 0, More = 1 };
enum class TerminationResponse { Wait = 0, Terminate = 1 };

struct GenericSubscriber {};

template <typename ENTRY>
struct GenericEntrySubscriber : GenericSubscriber {};

template <typename IMPL, typename ENTRY>
class EntrySubscriber : public GenericEntrySubscriber<ENTRY>, public IMPL {
 public:
  template <typename... ARGS>
  EntrySubscriber(ARGS&&... args) : IMPL(std::forward<ARGS>(args)...) {}
  virtual ~EntrySubscriber() {}

  template <typename CLONER = current::DefaultCloner>
  EntryResponse operator()(const ENTRY& e, IndexAndTimestamp current, IndexAndTimestamp last) {
    return IMPL::operator()(e, current, last);
  }
  EntryResponse operator()(ENTRY&& e, IndexAndTimestamp current, IndexAndTimestamp last) {
    return IMPL::operator()(std::move(e), current, last);
  }

  TerminationResponse Terminate() {
    return IMPL::Terminate();
  }
};

template <typename ENTRY>
struct GenericStreamSubscriber {};

template <typename IMPL, typename ENTRY>
class StreamSubscriber : public GenericStreamSubscriber<ENTRY>, public EntrySubscriber<IMPL, ENTRY> {
 public: 
  using EntrySubscriber<IMPL, ENTRY>::EntrySubscriber;
};

template <typename T>
struct IsSubscriber {
  static constexpr bool value = std::is_base_of<GenericSubscriber, T>::value;
};

template <typename T, typename E>
struct IsEntrySubscriber {
  static constexpr bool value = std::is_base_of<GenericEntrySubscriber<E>, T>::value;
};

template <typename T, typename E>
struct IsStreamSubscriber {
  static constexpr bool value = std::is_base_of<GenericStreamSubscriber<E>, T>::value;
};

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_SS_H
