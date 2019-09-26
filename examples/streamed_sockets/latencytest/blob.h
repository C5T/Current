/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2019 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_BLOB_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_BLOB_H

#include <cstddef>
#include <cstdint>

namespace current::examples::streamed_sockets {

struct Blob {
  uint64_t index;
  uint64_t request_origin;
  uint64_t request_sequence_id;
  uint64_t unused_extra_8bytes;
};

static_assert(sizeof(Blob) == 32);
static_assert((sizeof(Blob) & (sizeof(Blob) - 1u)) == 0u, "`sizeof(Blob)` should be a power of two for this example.");
static_assert(offsetof(Blob, index) == 0);
static_assert(offsetof(Blob, request_origin) == 8);

// clang-format off
constexpr static uint64_t request_origin_range_lo    = 0x00000000ff;
constexpr static uint64_t request_origin_range_hi    = 0x00ffffffff;
constexpr static uint64_t request_origin_latencytest = 0xaaffffffff;
// clang-format on

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_BLOB_H
