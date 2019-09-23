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

#ifndef EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H
#define EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H

#include <cstdlib>
#include <iostream>

#include "../blob.h"

#include "../../../../bricks/time/chrono.h"

namespace current::examples::streamed_sockets {

struct ProcessingWorker {
  ProcessingWorker() {}  // uint64_t max_index_block) : max_index_block(max_index_block) {}

  uint64_t next_expected_total_index = static_cast<uint64_t>(-1);
  const Blob* DoWork(Blob* begin, Blob* end) {
    while (begin != end) {
      if (next_expected_total_index == static_cast<uint64_t>(-1)) {
        next_expected_total_index = begin->index;
      } else if (begin->index != next_expected_total_index) {
        std::cerr << "Broken index continuity; exiting.\n";
        std::exit(-1);
      }
      if (begin->request_origin == request_origin_latencytest) {
        // std::cerr << current::time::Now().count() << '\t' << begin->request_sequence_id << '\n';
      }
      ++begin;
      ++next_expected_total_index;
    }
    return end;
  }
};

}  // namespace current::examples::streamed_sockets

#endif  // EXAMPLES_STREAMED_SOCKETS_LATENCYTEST_WORKERS_PROCESSOR_H
