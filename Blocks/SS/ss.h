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
  inline void PublishReplayed(const ENTRY& e, IndexAndTimestamp idx_ts) { IMPL::DoPublishReplayed(e, idx_ts); }
  inline void PublishReplayed(ENTRY&& e, IndexAndTimestamp idx_ts) {
    IMPL::DoPublishReplayed(std::move(e), idx_ts);
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

template <typename T, typename E>
struct IsStreamPublisher {
  static constexpr bool value = std::is_base_of<GenericStreamPublisher<E>, T>::value;
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

  EntryResponse operator()(const ENTRY& e, IndexAndTimestamp current, IndexAndTimestamp last) {
    return IMPL::operator()(e, current, last);
  }
  EntryResponse operator()(ENTRY&& e, IndexAndTimestamp current, IndexAndTimestamp last) {
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
