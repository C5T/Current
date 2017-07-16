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

// NOTE: `Persister` is the underlying synchronous storage component.
// It is effectively an STL container that is stored on disk.
// It is NOT the publish-subscribe-friendly publisher. See `Blocks/SS/pubsub.h` and `class Stream` for those.

#ifndef BLOCKS_SS_PERSISTER_H
#define BLOCKS_SS_PERSISTER_H

#include "idx_ts.h"
#include "types.h"

#include "../../bricks/sync/locks.h"
#include "../../bricks/time/chrono.h"

namespace current {
namespace ss {

enum class IterationMode : bool { Safe = true, Unsafe = false };

struct GenericPersister {};

template <typename ENTRY>
struct GenericEntryPersister : GenericPersister {};

template <typename IMPL, typename ENTRY>
class EntryPersister : public GenericEntryPersister<ENTRY>, public IMPL {
 public:
  template <IterationMode IM>
  using IterableRange = typename IMPL::template IterableRange<IM>;

  template <typename... ARGS>
  explicit EntryPersister(std::mutex& mutex, ARGS&&... args)
      : IMPL(mutex, std::forward<ARGS>(args)...) {}
  virtual ~EntryPersister() {}

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock,
            typename E,
            class = ENABLE_IF<CanPublish<current::decay<E>, ENTRY>::value>>
  idxts_t Publish(E&& e, current::time::DefaultTimeArgument = current::time::DefaultTimeArgument()) {
    return IMPL::template PersisterPublishImpl<MLS>(std::forward<E>(e), current::time::DefaultTimeArgument());
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock,
            typename E,
            class = ENABLE_IF<CanPublish<current::decay<E>, ENTRY>::value>>
  idxts_t Publish(E&& e, std::chrono::microseconds us) {
    return IMPL::template PersisterPublishImpl<MLS>(std::forward<E>(e), us);
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void UpdateHead(current::time::DefaultTimeArgument = current::time::DefaultTimeArgument()) {
    return IMPL::template PersisterUpdateHeadImpl<MLS>(current::time::DefaultTimeArgument());
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void UpdateHead(std::chrono::microseconds us) {
    return IMPL::template PersisterUpdateHeadImpl<MLS>(us);
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  bool Empty() const {
    return IMPL::template PersisterEmptyImpl<MLS>();
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  uint64_t Size() const {
    return IMPL::template PersisterSizeImpl<MLS>();
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  std::chrono::microseconds CurrentHead() const {
    return IMPL::template PersisterCurrentHeadImpl<MLS>();
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  idxts_t LastPublishedIndexAndTimestamp() const {
    return IMPL::template PersisterLastPublishedIndexAndTimestampImpl<MLS>();
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  head_optidxts_t HeadAndLastPublishedIndexAndTimestamp() const {
    return IMPL::template PersisterHeadAndLastPublishedIndexAndTimestampImpl<MLS>();
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  std::pair<uint64_t, uint64_t> IndexRangeByTimestampRange(std::chrono::microseconds from,
                                                           std::chrono::microseconds till) const {
    return IMPL::template PersisterIndexRangeByTimestampRangeImpl<MLS>(from, till);
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock,
            IterationMode IM = IterationMode::Safe>
  IterableRange<IM> Iterate(uint64_t begin = static_cast<uint64_t>(0), uint64_t end = static_cast<size_t>(-1)) const {
    return IMPL::template PersisterIterateImpl<MLS, IM>(begin, end);
  }

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock,
            IterationMode IM = IterationMode::Safe>
  IterableRange<IM> Iterate(std::chrono::microseconds from,
                            std::chrono::microseconds till = std::chrono::microseconds(-1)) const {
    return IMPL::template PersisterIterateImpl<MLS, IM>(from, till);
  }
};

// For `static_assert`-s.
template <typename T>
struct IsPersister {
  static constexpr bool value = std::is_base_of<GenericPersister, T>::value;
};

template <typename T, typename E>
struct IsEntryPersister {
  static constexpr bool value = std::is_base_of<GenericEntryPersister<E>, T>::value;
};

}  // namespace current::ss
}  // namespace current

#endif  // BLOCKS_SS_PERSISTER_H
