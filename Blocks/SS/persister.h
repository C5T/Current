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

#ifndef BLOCKS_SS_PERSISTER_H
#define BLOCKS_SS_PERSISTER_H

#include "idx_ts.h"

#include "../../Bricks/sync/locks.h"
#include "../../Bricks/time/chrono.h"

namespace current {
namespace ss {

struct GenericPersister {};

template <typename ENTRY>
struct GenericEntryPersister : GenericPersister {};

template <typename IMPL, typename ENTRY>
class EntryPersister : public GenericEntryPersister<ENTRY>, public IMPL {
 public:
  template <typename... ARGS>
  explicit EntryPersister(ARGS&&... args)
      : IMPL(std::forward<ARGS>(args)...) {}
  virtual ~EntryPersister() {}

  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  IndexAndTimestamp Publish(const ENTRY& e, std::chrono::microseconds us = current::time::Now()) {
    return IMPL::template DoPublish<MLS>(e, us);
  }
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  IndexAndTimestamp Publish(ENTRY&& e, std::chrono::microseconds us = current::time::Now()) {
    return IMPL::template DoPublish<MLS>(std::move(e), us);
  }
  template <current::locks::MutexLockStatus MLS = current::locks::MutexLockStatus::NeedToLock>
  void UpdateHead(std::chrono::microseconds us = current::time::Now()) {
    return IMPL::template DoUpdateHead<MLS>(us);
  }

  // template <typename... ARGS>
  // IndexAndTimestamp Emplace(ARGS&&... args) {
  //   return IMPL::DoEmplace(std::forward<ARGS>(args)...);
  // }

  bool Empty() const noexcept { return IMPL::Empty(); }
  uint64_t Size() const noexcept { return IMPL::Size(); }

  idxts_t LastPublishedIndexAndTimestamp() const { return IMPL::LastPublishedIndexAndTimestamp(); }
  std::chrono::microseconds CurrentHead() const { return IMPL::CurrentHead(); }
  std::pair<uint64_t, uint64_t> IndexRangeByTimestampRange(std::chrono::microseconds from,
                                                           std::chrono::microseconds till) const {
    return IMPL::IndexRangeByTimestampRange(from, till);
  }
  using IterableRange = typename IMPL::IterableRange;

  // NOTE: `IMPL::Iterate()` may throw.
  IterableRange Iterate(uint64_t begin, uint64_t end) const { return IMPL::Iterate(begin, end); }
  IterableRange Iterate(uint64_t begin) const { return IMPL::Iterate(begin, static_cast<uint64_t>(-1)); }
  IterableRange Iterate(std::chrono::microseconds from, std::chrono::microseconds till) const {
    return IMPL::Iterate(from, till);
  }
  IterableRange Iterate(std::chrono::microseconds from) const {
    return IMPL::Iterate(from, std::chrono::microseconds(-1));
  }
  IterableRange Iterate() const { return IMPL::Iterate(0, static_cast<uint64_t>(-1)); }
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
