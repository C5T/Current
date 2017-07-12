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

#ifndef BLOCKS_SS_IDX_TS_H
#define BLOCKS_SS_IDX_TS_H

#include "../../port.h"

#ifdef CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
#include <iostream>
#endif  // CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS

#include <chrono>

#include "exceptions.h"

#include "../../TypeSystem/optional.h"
#include "../../TypeSystem/struct.h"

namespace current {
namespace ss {

// The `IndexAndTimestamp` binds record index and timestamp together.
// * `index` is 0-based.
// * `us` is an epoch microsecond timestamp.
CURRENT_STRUCT(IndexAndTimestamp) {
  CURRENT_FIELD(index, uint64_t);
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_USE_FIELD_AS_TIMESTAMP(us);
  CURRENT_DEFAULT_CONSTRUCTOR(IndexAndTimestamp) : index(0u), us(0) {}
  CURRENT_CONSTRUCTOR(IndexAndTimestamp)(uint64_t index, std::chrono::microseconds us) : index(index), us(us) {}
};

// An index and timestamp of an entry or a timestamp of the current head.
// In the latter case the `index` member is unset.
CURRENT_STRUCT(TimestampAndOptionalIndex) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(index, Optional<uint64_t>);
  CURRENT_USE_FIELD_AS_TIMESTAMP(us);
  CURRENT_DEFAULT_CONSTRUCTOR(TimestampAndOptionalIndex) : us(0) {}
  CURRENT_CONSTRUCTOR(TimestampAndOptionalIndex)(std::chrono::microseconds us) : us(us) {}
  CURRENT_CONSTRUCTOR(TimestampAndOptionalIndex)(std::chrono::microseconds us, uint64_t index) : us(us), index(index) {}
};

// The `head` is the timestamp of either the last published entry or the last `UpdateHead()` call,
// depending on whether the most recent update was `.Publish()` or `.UpdateHead()`.
// The `idxts` stores the timestamp and index of the last published entry, it is unset if there were no entries yet.
CURRENT_STRUCT(HeadAndOptionalIndexAndTimestamp) {
  CURRENT_FIELD(head, std::chrono::microseconds);
  CURRENT_FIELD(idxts, Optional<IndexAndTimestamp>);
  CURRENT_USE_FIELD_AS_TIMESTAMP(head);
  CURRENT_DEFAULT_CONSTRUCTOR(HeadAndOptionalIndexAndTimestamp) : head(0) {}
  CURRENT_CONSTRUCTOR(HeadAndOptionalIndexAndTimestamp)(std::chrono::microseconds head) : head(head) {}
  CURRENT_CONSTRUCTOR(HeadAndOptionalIndexAndTimestamp)(std::chrono::microseconds head, IndexAndTimestamp idxts)
      : head(head), idxts(idxts) {
    CURRENT_ASSERT(head >= idxts.us);
  }
  CURRENT_CONSTRUCTOR(HeadAndOptionalIndexAndTimestamp)(
      std::chrono::microseconds head, uint64_t index, std::chrono::microseconds us)
      : head(head), idxts(IndexAndTimestamp(index, us)) {
#ifndef CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
    CURRENT_ASSERT(head >= us);
#else
    if (!(head >= us)) {
      std::cerr << "head: " << head.count() << ", us: " << us.count() << std::endl;
      CURRENT_ASSERT(head >= us);
    }
#endif  // CURRENT_BUILD_WITH_PARANOIC_RUNTIME_CHECKS
  }
};

}  // namespace current::ss
}  // namespace current

using idxts_t = current::ss::IndexAndTimestamp;
using ts_optidx_t = current::ss::TimestampAndOptionalIndex;
using head_optidxts_t = current::ss::HeadAndOptionalIndexAndTimestamp;

#endif  // BLOCKS_SS_IDX_TS_H
