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

#include <chrono>

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

// The same as `IndexAndTimestamp`, but with optional index member.
CURRENT_STRUCT(TimestampAndOptionalIndex) {
  CURRENT_FIELD(us, std::chrono::microseconds);
  CURRENT_FIELD(index, Optional<uint64_t>);
  CURRENT_USE_FIELD_AS_TIMESTAMP(us);
  CURRENT_DEFAULT_CONSTRUCTOR(TimestampAndOptionalIndex) : us(0) {}
  CURRENT_CONSTRUCTOR(TimestampAndOptionalIndex)(std::chrono::microseconds us) : us(us) {}
  CURRENT_CONSTRUCTOR(TimestampAndOptionalIndex)(std::chrono::microseconds us, uint64_t index) : us(us), index(index) {}
};

}  // namespace current::ss
}  // namespace current

using idxts_t = current::ss::IndexAndTimestamp;
using ts_optidx_t = current::ss::TimestampAndOptionalIndex;

#endif  // BLOCKS_SS_IDX_TS_H
