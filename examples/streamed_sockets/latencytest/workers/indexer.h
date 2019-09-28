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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_INDEXER_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_INDEXER_H

#include "../blob.h"

namespace current::examples::streamed_sockets {

struct IndexingWorker final {
  uint64_t total_index = 0u;
  const Blob* DoWork(Blob* begin, Blob* end) {
    // NOTE(dkorolev): Index in blocks of the size up to 32KB.
    constexpr static size_t block_size_in_blobs = (1 << 15) / sizeof(Blob);
    static_assert(block_size_in_blobs > 0);
    if (end > begin + block_size_in_blobs) {
      end = begin + block_size_in_blobs;
    }
    while (begin != end) {
      (*begin++).index = total_index++;
    }
    return end;
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_INDEXER_H
